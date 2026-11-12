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
#include "NextionInternal.hpp"
#include <stdio.h>
#include <string.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/util.h>

LOG_MODULE_DECLARE(nextion);

using namespace Nextion::Detail;
using Nextion::writeCommand;
using Nextion::writeRaw;

namespace
{
    constexpr uint32_t ACK_TIMEOUT_MS = 5000;
    constexpr uint32_t RESET_TIMEOUT_MS = 5000;
    constexpr size_t PACKET_SIZE = 4096;

    // After the link baud, others a display UI may have been built with.
    constexpr uint32_t OTHER_BAUDS[] = {115200, 9600};
    constexpr uint32_t CONNECT_PROBE_TIMEOUT_MS = 1200;

    constexpr uint8_t TWFILE_MAGIC[] = {0x3A, 0xA1, 0xBB, 0x44, 0x7F, 0xFF, 0xFE};
    constexpr uint32_t TWFILE_ACK_TIMEOUT_MS = 500;
    constexpr int TWFILE_ATTEMPTS = 3;

    uint8_t g_packet[PACKET_SIZE];
    size_t g_packetLen = 0;
    uint32_t g_totalLen = 0;
    uint32_t g_sentLen = 0;

    // "connect" answers with text containing "comok" and its own terminator,
    // which must be drained too or it leaks into the next ack wait.
    bool waitForConnectOk(uint32_t timeoutMs)
    {
        char buf[32];
        size_t len = 0;
        bool found = false;
        int ffRun = 0;
        uint32_t start = k_uptime_get_32();
        while (k_uptime_get_32() - start < timeoutMs) {
            uint8_t b;
            if (!readByte(b, 20)) {
                continue;
            }
            if (b == 0xFF) {
                ffRun++;
                if (found && ffRun >= 3) {
                    return true;
                }
                continue;
            }
            ffRun = 0;
            if (len < sizeof(buf) - 1) {
                buf[len++] = (char)b;
                buf[len] = '\0';
            } else {
                memmove(buf, buf + 1, sizeof(buf) - 1);
                buf[sizeof(buf) - 2] = (char)b;
            }
            if (!found && strstr(buf, "comok") != nullptr) {
                found = true;
            }
        }
        return found;
    }

    bool tryConnect(uint32_t baud)
    {
        LOG_INF("trying connect at %u baud", baud);
        if (!setBaud(baud)) {
            LOG_INF("uart_configure(%u) failed", baud);
            return false;
        }
        drainInput();
        writeTerminator();
        k_sleep(K_MSEC(5));
        drainInput();
        writeCommand("connect");
        if (waitForConnectOk(CONNECT_PROBE_TIMEOUT_MS)) {
            return true;
        }
        LOG_INF("no reply at %u baud", baud);
        return false;
    }

    // The baud the display answered "connect" at, or 0.
    uint32_t connectAnyBaud()
    {
        if (tryConnect(config().linkBaud)) {
            return config().linkBaud;
        }
        for (uint32_t baud : OTHER_BAUDS) {
            if (tryConnect(baud)) {
                return baud;
            }
        }
        return 0;
    }

    void endFlash(bool restarted)
    {
        g_packetLen = 0;
        setBaud(config().linkBaud);
        release(restarted);
    }

    bool flushPacket()
    {
        if (g_packetLen == 0) {
            return true;
        }
        writeBytes(g_packet, g_packetLen);
        bool ok = waitForByte(0x05, ACK_TIMEOUT_MS);
        if (ok) {
            g_sentLen += g_packetLen;
        }
        g_packetLen = 0;
        return ok;
    }

    // A reply is its code then 0xFF 0xFF 0xFF; returns the code, or -1.
    int readReply(uint32_t timeoutMs)
    {
        uint8_t code;
        if (!readByte(code, timeoutMs)) {
            return -1;
        }
        for (int i = 0; i < 3; i++) {
            uint8_t b;
            if (!readByte(b, timeoutMs) || b != 0xFF) {
                return -1;
            }
        }
        return code;
    }

    void writeTwfileHeader(uint16_t id, uint16_t len)
    {
        writeBytes(TWFILE_MAGIC, sizeof(TWFILE_MAGIC));
        const uint8_t header[] = {0x00 /* no CRC */, (uint8_t)(id & 0xFF), (uint8_t)(id >> 8), (uint8_t)(len & 0xFF),
                                  (uint8_t)(len >> 8)};
        writeBytes(header, sizeof(header));
    }

    // The last packet is answered with the transfer's end (0xFD) instead of an ack.
    bool sendTwfilePacket(uint16_t id, const uint8_t *data, uint16_t len, bool last)
    {
        for (int attempt = 0; attempt < TWFILE_ATTEMPTS; attempt++) {
            if (attempt) {
                k_sleep(K_MSEC(25)); // a >20 ms gap discards a partial packet
                drainInput();
            }
            writeTwfileHeader(id, len);
            writeBytes(data, len);
            uint8_t b = 0;
            bool got = readByte(b, TWFILE_ACK_TIMEOUT_MS);
            if (got && b == 0x05) {
                return true;
            }
            if (got && b == 0xFD && last) {
                uint8_t t;
                for (int i = 0; i < 3; i++) {
                    readByte(t, TWFILE_ACK_TIMEOUT_MS);
                }
                return true;
            }
            LOG_INF("twfile packet %u not acked: %s 0x%02x", id, got ? "got" : "timeout", b);
        }
        return false;
    }

    bool sendFile(const char *path, const uint8_t *data, size_t len)
    {
        char cmd[64];
        snprintf(cmd, sizeof(cmd), "delfile \"%s\"", path);
        writeRaw(cmd);
        k_sleep(K_MSEC(50));
        drainInput();

        // No heartbeats mid-transfer; a negative age is grace before the display's lost-link toast.
        writeRaw("linkAge=-20");
        snprintf(cmd, sizeof(cmd), "twfile \"%s\",%u", path, (unsigned)len);
        writeRaw(cmd);
        int reply = readReply(TWFILE_ACK_TIMEOUT_MS);
        if (reply != 0xFE) {
            LOG_INF("twfile refused: 0x%02x", reply);
            return false;
        }
        size_t sent = 0;
        uint16_t id = 0;
        while (sent < len) {
            uint16_t chunk = (uint16_t)MIN(len - sent, PACKET_SIZE);
            if (!sendTwfilePacket(id++, data + sent, chunk, sent + chunk == len)) {
                k_sleep(K_MSEC(25));
                writeTwfileHeader(0xFFFF, 0);
                readReply(TWFILE_ACK_TIMEOUT_MS);
                return false;
            }
            sent += chunk;
        }
        return true;
    }
}

namespace Nextion
{
    bool flashBegin(uint32_t totalLen)
    {
        seize();
        g_totalLen = totalLen;
        g_sentLen = 0;
        g_packetLen = 0;

        uint32_t connectBaud = connectAnyBaud();
        if (connectBaud == 0) {
            LOG_INF("could not connect at any baud");
            return false;
        }
        LOG_INF("connected at %u baud", connectBaud);

        char cmd[48];
        snprintf(cmd, sizeof(cmd), "whmi-wri %u,%u,0", totalLen, config().uploadBaud);
        drainInput();
        writeTerminator();
        writeCommand(cmd);
        if (connectBaud != config().uploadBaud) {
            k_sleep(K_MSEC(60));
            if (!setBaud(config().uploadBaud)) {
                LOG_INF("could not switch to upload baud");
                return false;
            }
        }
        if (!waitForByte(0x05, ACK_TIMEOUT_MS)) {
            LOG_INF("whmi-wri not accepted");
            return false;
        }
        LOG_INF("upload accepted, %u bytes", totalLen);
        return true;
    }

    bool flashChunk(const uint8_t *data, size_t len)
    {
        size_t offset = 0;
        while (offset < len) {
            size_t n = MIN(len - offset, PACKET_SIZE - g_packetLen);
            memcpy(g_packet + g_packetLen, data + offset, n);
            g_packetLen += n;
            offset += n;
            if (g_packetLen == PACKET_SIZE && !flushPacket()) {
                LOG_INF("packet not acked");
                return false;
            }
        }
        return true;
    }

    bool flashFinish()
    {
        if (!flushPacket()) {
            LOG_INF("final packet not acked");
            return false;
        }
        if (g_sentLen != g_totalLen) {
            LOG_INF("size mismatch, sent %u of %u", g_sentLen, g_totalLen);
            return false;
        }
        // The restarted display reports at its UI baud.
        setBaud(config().linkBaud);
        if (!waitForByte(0x88, RESET_TIMEOUT_MS)) {
            LOG_INF("no reset after upload");
            return false;
        }
        LOG_INF("flashed");
        endFlash(true);
        return true;
    }

    void flashAbort()
    {
        endFlash(true);
    }

    bool twfile(const char *path, const uint8_t *data, size_t len)
    {
        drainInput();
        return sendFile(path, data, len);
    }
}
