/*
 * This file is part of the ScrewSync project.
 * Copyright (c) 2026 Donatas Olsevičius
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Affero General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * Affero General Public License for more details.
 *
 * You should have received a copy of the GNU Affero General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */
#include "WsCommand.hpp"

#include <string.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/net/socket.h>
#include <zephyr/net/websocket.h>
#include <zephyr/sys/util.h>
#include "Command.hpp"
#include "Settings.hpp"
#include "Wifi.hpp"
#include "Machine.hpp"
#include "WebHttp.hpp"
#include "WsMessages.hpp"

extern "C" {
#include "websocket_internal.h"
}

LOG_MODULE_REGISTER(ws_command, LOG_LEVEL_INF);

namespace
{
    // An upgraded websocket gives its HTTP client slot back, so only websocket contexts limit this.
    constexpr int WS_MAX_CLIENTS = CONFIG_WEBSOCKET_MAX_CONTEXTS;
    constexpr size_t WS_STACK_SIZE = 4096;
    constexpr size_t WS_RX_BUF = 256;
    constexpr size_t JSON_REPLY_BUF = Command::MAX_REPLY_LEN + 512;
    constexpr size_t JSON_BIG_BUF = 2048;
    // Browsers answer pings on their own; a client silent past the timeout is gone.
    constexpr uint32_t PING_INTERVAL_MS = 2000;
    constexpr uint32_t SILENT_TIMEOUT_MS = 6000;

    struct WsClient
    {
        int sock = -1;
        uint32_t connectedMs = 0;
        uint32_t heardMs = 0;
        uint32_t rxCount = 0;
        bool kill = false;
        char peer[NET_IPV6_ADDR_LEN + 8] = "?";
    };

    struct ContextSetup
    {
        int wsSock;
        uint8_t *recvBuf;
        int realSock;
    };

    // The HTTP server registers every websocket with the resource's one
    // buffer, which the frame parser keeps state in; give each its own.
    void setupContext(struct websocket_context *ctx, void *user_data)
    {
        ContextSetup *setup = (ContextSetup *)user_data;
        if (ctx->sock != setup->wsSock) {
            return;
        }
        k_mutex_lock(&ctx->lock, K_FOREVER);
        size_t count = MIN(ctx->recv_buf.count, (size_t)WS_RX_BUF);
        memcpy(setup->recvBuf, ctx->recv_buf.buf, count);
        ctx->recv_buf.buf = setup->recvBuf;
        ctx->recv_buf.size = WS_RX_BUF;
        ctx->recv_buf.count = count;
        k_mutex_unlock(&ctx->lock);
        setup->realSock = ctx->real_sock;
    }

    // Moves the websocket onto recvBuf; returns its TCP socket, or -1 if not found.
    int setupClient(int wsSock, uint8_t *recvBuf)
    {
        ContextSetup setup = {wsSock, recvBuf, -1};
        websocket_context_foreach(setupContext, &setup);
        return setup.realSock;
    }

    // "ip:port" of the peer on realSock, or "?".
    void peerAddress(int realSock, char *out, size_t outMax)
    {
        snprintf(out, outMax, "?");
        if (realSock < 0) {
            return;
        }
        struct sockaddr_storage addr;
        socklen_t addrLen = sizeof(addr);
        if (zsock_getpeername(realSock, (struct sockaddr *)&addr, &addrLen) != 0) {
            return;
        }
        char ip[NET_IPV6_ADDR_LEN];
        if (addr.ss_family == AF_INET)
        {
            struct sockaddr_in *in = (struct sockaddr_in *)&addr;
            zsock_inet_ntop(AF_INET, &in->sin_addr, ip, sizeof(ip));
            snprintf(out, outMax, "%s:%u", ip, ntohs(in->sin_port));
        }
        else if (addr.ss_family == AF_INET6)
        {
            struct sockaddr_in6 *in6 = (struct sockaddr_in6 *)&addr;
            zsock_inet_ntop(AF_INET6, &in6->sin6_addr, ip, sizeof(ip));
            snprintf(out, outMax, "[%s]:%u", ip, ntohs(in6->sin6_port));
        }
    }
    WsClient g_clients[WS_MAX_CLIENTS];
    K_MUTEX_DEFINE(g_clientsLock);

    K_THREAD_STACK_ARRAY_DEFINE(g_wsStacks, WS_MAX_CLIENTS, WS_STACK_SIZE);
    struct k_thread g_wsThreads[WS_MAX_CLIENTS];
    struct k_sem g_wsReady[WS_MAX_CLIENTS];
    uint8_t g_wsUpgradeBuf[WS_RX_BUF];
    uint8_t g_wsRecv[WS_MAX_CLIENTS][WS_RX_BUF] EXT_RAM_BSS;

    uint8_t g_rx[WS_MAX_CLIENTS][WS_RX_BUF] EXT_RAM_BSS;
    char g_line[WS_MAX_CLIENTS][WS_RX_BUF] EXT_RAM_BSS;
    char g_reply[WS_MAX_CLIENTS][Command::MAX_REPLY_LEN] EXT_RAM_BSS;
    char g_json[WS_MAX_CLIENTS][JSON_REPLY_BUF] EXT_RAM_BSS;
    char g_bigJson[WS_MAX_CLIENTS][JSON_BIG_BUF] EXT_RAM_BSS;

    void sendTo(int sock, const char *json, int len)
    {
        if (len <= 0) {
            return;
        }
        websocket_send_msg(sock, (const uint8_t *)json, (size_t)len, WEBSOCKET_OPCODE_DATA_TEXT, false, true, 1000);
    }

    void broadcast(const char *json, int len)
    {
        if (len <= 0) {
            return;
        }
        int socks[WS_MAX_CLIENTS];
        int count = 0;
        k_mutex_lock(&g_clientsLock, K_FOREVER);
        for (int i = 0; i < WS_MAX_CLIENTS; i++) {
            if (g_clients[i].sock >= 0) {
                socks[count++] = g_clients[i].sock;
            }
        }
        k_mutex_unlock(&g_clientsLock);
        for (int i = 0; i < count; i++) {
            sendTo(socks[i], json, len);
        }
    }

    void firstToken(const char *s, char *tok, size_t tokMax)
    {
        while (*s == ' ') {
            s++;
        }
        size_t i = 0;
        while (s[i] && s[i] != ' ' && i < tokMax - 1) {
            tok[i] = s[i];
            i++;
        }
        tok[i] = '\0';
    }

    bool isStatusVerb(const char *verb)
    {
        static const char *const VERBS[] = {"jog",    "mode",  "start", "stop", "skip",    "gear",  "async",
                                            "turn",   "thread", "part", "slot", "ellipse", "units", "enable",
                                            "move",   "zero"};
        for (const char *v : VERBS) {
            if (!strcmp(verb, v)) {
                return true;
            }
        }
        return false;
    }

    int currentSock(int slot)
    {
        k_mutex_lock(&g_clientsLock, K_FOREVER);
        int sock = g_clients[slot].sock;
        k_mutex_unlock(&g_clientsLock);
        return sock;
    }

    struct Session
    {
        int slot;
        int sock;
        char *json;
        char *bigJson;
        uint32_t lastStatusMs;
        uint32_t lastPingMs;
        bool wasScanBusy;

        bool statusDue(uint32_t now) const
        {
            return now - lastStatusMs >= (uint32_t)Settings::active.statusPushIntervalMs;
        }

        void greet()
        {
            sendTo(sock, json, WsMessages::status(json, JSON_REPLY_BUF));
            sendTo(sock, bigJson, WsMessages::settings(bigJson, JSON_BIG_BUF));
            sendTo(sock, json, WsMessages::wifi(json, JSON_REPLY_BUF));
        }

        // Pings and periodic pushes; false once the client should be closed.
        bool idle()
        {
            uint32_t now = k_uptime_get_32();
            if (g_clients[slot].kill)
            {
                LOG_INF("ws: slot %d killed", slot);
                return false;
            }
            if (now - g_clients[slot].heardMs > SILENT_TIMEOUT_MS)
            {
                LOG_WRN("ws: slot %d silent for %u ms, closing", slot, now - g_clients[slot].heardMs);
                return false;
            }
            if (now - lastPingMs >= PING_INTERVAL_MS)
            {
                websocket_send_msg(sock, nullptr, 0, WEBSOCKET_OPCODE_PING, false, true, 1000);
                lastPingMs = now;
            }
            if (statusDue(now))
            {
                sendTo(sock, json, WsMessages::status(json, JSON_REPLY_BUF));
                bool scanBusy = Wifi::scanBusy();
                if (wasScanBusy && !scanBusy) {
                    broadcast(bigJson, WsMessages::scan(bigJson, JSON_BIG_BUF));
                }
                wasScanBusy = scanBusy;
                lastStatusMs = now;
            }
            return true;
        }

        // Errors and query replies go back to this client; changes are broadcast as fresh state.
        void answer(const char *line, const char *reply)
        {
            char verb[16];
            firstToken(line, verb, sizeof(verb));
            const char *sp = strchr(reply, ' ');
            if (strncmp(reply, "ok", 2)) {
                sendTo(sock, json, WsMessages::typed(json, JSON_REPLY_BUF, "error", sp ? sp + 1 : reply));
            } else if (!strcmp(verb, "commands") || !strcmp(verb, "display") || !strcmp(verb, "ws")) {
                sendTo(sock, json, WsMessages::typed(json, JSON_REPLY_BUF, verb, sp ? sp + 1 : ""));
            } else if (!strcmp(verb, "wifi")) {
                broadcast(json, WsMessages::wifi(json, JSON_REPLY_BUF));
            } else if (!strcmp(verb, "settings")) {
                broadcast(bigJson, WsMessages::settings(bigJson, JSON_BIG_BUF));
            } else if (isStatusVerb(verb)) {
                // Same throttle as idle(): a burst of commands must not raise the status rate.
                uint32_t now = k_uptime_get_32();
                if (statusDue(now))
                {
                    broadcast(json, WsMessages::status(json, JSON_REPLY_BUF));
                    lastStatusMs = now;
                }
            }
        }

        void received(uint8_t *rx, int n)
        {
            LOG_INF("ws: slot %d recv %d bytes: %.*s", slot, n, n, (const char *)rx);
            const char *requested;
            if (!WsMessages::parseCommand((char *)rx, (size_t)n, requested))
            {
                LOG_WRN("ws: slot %d bad request", slot);
                sendTo(sock, json, WsMessages::typed(json, JSON_REPLY_BUF, "error", "bad request"));
                return;
            }
            char *line = g_line[slot];
            char *reply = g_reply[slot];
            strncpy(line, requested, WS_RX_BUF - 1);
            line[WS_RX_BUF - 1] = '\0';
            Command::apply(line, reply, Command::MAX_REPLY_LEN);
            answer(line, reply);
        }
    };

    void serve(Session &s)
    {
        uint8_t *rx = g_rx[s.slot];
        s.greet();
        while (s.sock >= 0)
        {
            uint32_t msgType = 0;
            uint64_t remaining = 0;
            int n = websocket_recv_msg(s.sock, rx, WS_RX_BUF, &msgType, &remaining, 20);
            if (n == -EAGAIN)
            {
                if (!s.idle()) {
                    return;
                }
            }
            else if (n < 0 || (msgType & WEBSOCKET_FLAG_CLOSE))
            {
                LOG_INF("ws: slot %d recv ended (n=%d, msgType=0x%x)", s.slot, n, msgType);
                return;
            }
            else
            {
                g_clients[s.slot].heardMs = k_uptime_get_32();
                g_clients[s.slot].rxCount++;
                if ((msgType & WEBSOCKET_FLAG_TEXT) && n > 0) {
                    s.received(rx, n);
                }
            }
            s.sock = currentSock(s.slot);
        }
    }

    void wsWorker(void *p1, void *, void *)
    {
        int slot = (int)(intptr_t)p1;
        for (;;)
        {
            k_sem_take(&g_wsReady[slot], K_FOREVER);
            uint32_t now = k_uptime_get_32();
            Session s = {slot,          g_clients[slot].sock, g_json[slot], g_bigJson[slot],
                         now - Settings::active.statusPushIntervalMs, now, Wifi::scanBusy()};
            serve(s);

            k_mutex_lock(&g_clientsLock, K_FOREVER);
            if (g_clients[slot].sock >= 0) {
                websocket_unregister(g_clients[slot].sock);
            }
            g_clients[slot] = WsClient{};
            k_mutex_unlock(&g_clientsLock);
            LOG_INF("ws: slot %d freed", slot);
        }
    }

    int wsAccept(int ws_socket, struct http_request_ctx *, void *)
    {
        if (Machine::blocksTraffic()) {
            LOG_WRN("ws: accept rejected, machine state=%s", Machine::stateName());
            return -1;
        }

        k_mutex_lock(&g_clientsLock, K_FOREVER);
        int slot = -1;
        for (int i = 0; i < WS_MAX_CLIENTS; i++) {
            if (g_clients[i].sock < 0)
            {
                slot = i;
                break;
            }
        }
        if (slot >= 0)
        {
            uint32_t now = k_uptime_get_32();
            g_clients[slot] = WsClient{};
            g_clients[slot].sock = ws_socket;
            g_clients[slot].connectedMs = now;
            g_clients[slot].heardMs = now;
            peerAddress(setupClient(ws_socket, g_wsRecv[slot]), g_clients[slot].peer, sizeof(g_clients[slot].peer));
        }
        k_mutex_unlock(&g_clientsLock);

        if (slot < 0) {
            LOG_WRN("ws: accept rejected, all %d slots busy", WS_MAX_CLIENTS);
            return -1;
        }

        LOG_INF("ws: client %s accepted into slot %d (sock=%d)", g_clients[slot].peer, slot, ws_socket);
        k_sem_give(&g_wsReady[slot]);
        return 0;
    }
}

struct http_resource_detail_websocket wsCommandDetail = {
    .common = {.bitmask_of_supported_http_methods = BIT(HTTP_GET), .type = HTTP_RESOURCE_TYPE_WEBSOCKET},
    .ws_sock = -1, .cb = wsAccept, .data_buffer = g_wsUpgradeBuf, .data_buffer_len = WS_RX_BUF,
    .user_data = nullptr,
};

namespace WsCommand
{
    void begin()
    {
        for (int i = 0; i < WS_MAX_CLIENTS; i++)
        {
            k_sem_init(&g_wsReady[i], 0, 1);
            k_thread_create(&g_wsThreads[i], g_wsStacks[i], WS_STACK_SIZE, wsWorker,
                             (void *)(intptr_t)i, nullptr, nullptr, K_PRIO_PREEMPT(8), 0, K_NO_WAIT);
        }
    }

    void describeClients(char *out, size_t outMax)
    {
        uint32_t now = k_uptime_get_32();
        int n = snprintf(out, outMax, "ok");
        k_mutex_lock(&g_clientsLock, K_FOREVER);
        for (int i = 0; i < WS_MAX_CLIENTS && n >= 0 && (size_t)n < outMax; i++)
        {
            const WsClient &c = g_clients[i];
            if (c.sock < 0) {
                n += snprintf(out + n, outMax - (size_t)n, "\n%d: free", i + 1);
                continue;
            }
            n += snprintf(out + n, outMax - (size_t)n,
                          "\n%d: %s sock=%d connected=%us ago heard=%ums ago rx=%u%s", i + 1, c.peer, c.sock,
                          (now - c.connectedMs) / 1000, now - c.heardMs, c.rxCount, c.kill ? " closing" : "");
        }
        k_mutex_unlock(&g_clientsLock);
    }

    const char *kill(int number)
    {
        if (number < 1 || number > WS_MAX_CLIENTS) {
            return "no such client";
        }
        k_mutex_lock(&g_clientsLock, K_FOREVER);
        bool used = g_clients[number - 1].sock >= 0;
        if (used) {
            g_clients[number - 1].kill = true;
        }
        k_mutex_unlock(&g_clientsLock);
        return used ? nullptr : "no such client";
    }

    void closeAll()
    {
        int socks[WS_MAX_CLIENTS];
        int count = 0;
        k_mutex_lock(&g_clientsLock, K_FOREVER);
        for (int i = 0; i < WS_MAX_CLIENTS; i++) {
            if (g_clients[i].sock >= 0) {
                socks[count++] = g_clients[i].sock;
                g_clients[i] = WsClient{};
            }
        }
        k_mutex_unlock(&g_clientsLock);
        for (int i = 0; i < count; i++) {
            websocket_unregister(socks[i]);
        }
    }
}
