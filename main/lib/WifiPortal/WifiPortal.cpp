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
#include "WifiPortal.hpp"
#include <string.h>
#include <stdio.h>
#include <zephyr/kernel.h>
#include "Wifi.hpp"
#include "Machine.hpp"
#include "config.hpp"
#include "wifi_portal.hpp"

#ifdef CONFIG_ESP_SPIRAM
#define EXT_RAM_BSS __attribute__((section(".ext_ram.bss")))
#else
#define EXT_RAM_BSS
#endif

namespace
{
    char g_pendingSsid[33];
    char g_pendingPass[65];

    void applyPendingCredentials(struct k_work *)
    {
        Wifi::setCredentials(g_pendingSsid, g_pendingPass);
    }

    K_WORK_DELAYABLE_DEFINE(g_applyCredentialsWork, applyPendingCredentials);

    char hexVal(char c)
    {
        if (c >= '0' && c <= '9') {
            return c - '0';
        }
        if (c >= 'a' && c <= 'f') {
            return c - 'a' + 10;
        }
        if (c >= 'A' && c <= 'F') {
            return c - 'A' + 10;
        }
        return -1;
    }

    void urlDecode(const char *src, size_t len, char *out, size_t outMax)
    {
        size_t n = 0;
        for (size_t i = 0; i < len && n + 1 < outMax; i++)
        {
            char c = src[i];
            if (c == '+') {
                c = ' ';
            } else if (c == '%' && i + 2 < len) {
                char hi = hexVal(src[i + 1]), lo = hexVal(src[i + 2]);
                if (hi >= 0 && lo >= 0)
                {
                    c = (char)((hi << 4) | lo);
                    i += 2;
                }
            }
            out[n++] = c;
        }
        out[n] = '\0';
    }

    // application/x-www-form-urlencoded "s=<ssid>&p=<password>" (wifi_portal.hpp's form).
    bool parseField(const char *body, size_t len, const char *key, char *out, size_t outMax)
    {
        size_t keyLen = strlen(key);
        for (size_t i = 0; i + keyLen <= len; i++)
        {
            if ((i != 0 && body[i - 1] != '&') ||
                memcmp(body + i, key, keyLen) != 0 || i + keyLen >= len ||
                body[i + keyLen] != '=') {
                continue;
            }
            size_t start = i + keyLen + 1, end = start;
            while (end < len && body[end] != '&') {
                end++;
            }
            urlDecode(body + start, end - start, out, outMax);
            return true;
        }
        return false;
    }

    int wifiPortalHandler(struct http_client_ctx *client, enum http_transaction_status status,
                           const struct http_request_ctx *request_ctx,
                           struct http_response_ctx *response_ctx, void *)
    {
        static char body[256] EXT_RAM_BSS;
        static size_t cursor;

        if (status == HTTP_SERVER_TRANSACTION_ABORTED || status == HTTP_SERVER_TRANSACTION_COMPLETE)
        {
            cursor = 0;
            return 0;
        }

        if (Machine::blocksTraffic())
        {
            cursor = 0;
            static char msg[32];
            snprintf(msg, sizeof(msg), "%s\n", Machine::stateName());
            response_ctx->status = HTTP_503_SERVICE_UNAVAILABLE;
            response_ctx->body = (const uint8_t *)msg;
            response_ctx->body_len = strlen(msg);
            response_ctx->final_chunk = true;
            return 0;
        }

        if (client->method == HTTP_POST)
        {
            if (request_ctx->data_len + cursor < sizeof(body))
            {
                memcpy(body + cursor, request_ctx->data, request_ctx->data_len);
                cursor += request_ctx->data_len;
            }
        }

        if (status != HTTP_SERVER_REQUEST_DATA_FINAL) {
            return 0;
        }

        if (client->method == HTTP_POST)
        {
            char ssid[33] = {0};
            char pass[65] = {0};
            size_t n = cursor;
            cursor = 0;

            bool haveSsid = parseField(body, n, "s", ssid, sizeof(ssid));
            parseField(body, n, "p", pass, sizeof(pass));

            static char msg[64];
            if (!haveSsid || !ssid[0])
            {
                snprintf(msg, sizeof(msg), "SSID required\n");
                response_ctx->status = HTTP_400_BAD_REQUEST;
            }
            else
            {
                strncpy(g_pendingSsid, ssid, sizeof(g_pendingSsid) - 1);
                g_pendingSsid[sizeof(g_pendingSsid) - 1] = '\0';
                strncpy(g_pendingPass, pass, sizeof(g_pendingPass) - 1);
                g_pendingPass[sizeof(g_pendingPass) - 1] = '\0';
                k_work_schedule(&g_applyCredentialsWork, K_MSEC(500));
                snprintf(msg, sizeof(msg), "Saved -- connecting to '%s'\n", ssid);
                response_ctx->status = HTTP_200_OK;
            }
            response_ctx->body = (const uint8_t *)msg;
            response_ctx->body_len = strlen(msg);
            response_ctx->final_chunk = true;
            return 0;
        }

        if (Wifi::mode() != Wifi::Mode::AccessPoint)
        {
            static const struct http_header redirect[] = {{"Location", "/"}};
            response_ctx->status = HTTP_302_FOUND;
            response_ctx->headers = redirect;
            response_ctx->header_count = 1;
            response_ctx->final_chunk = true;
            return 0;
        }

        static char page[2048] EXT_RAM_BSS;
        char host[48];
        snprintf(host, sizeof(host), "%s.local", Wifi::mdnsHost());
        size_t n = 0;
        for (const char *src = WEB_WIFI_HTML; *src && n + 1 < sizeof(page);)
        {
            if (!strncmp(src, "{{MDNS_HOST}}", 13))
            {
                n += snprintf(page + n, sizeof(page) - n, "%s", host);
                src += 13;
            }
            else if (!strncmp(src, "{{DEVICE_NAME}}", 15))
            {
                n += snprintf(page + n, sizeof(page) - n, "%s", DEVICE_NAME);
                src += 15;
            } else {
                page[n++] = *src++;
            }
        }
        page[n < sizeof(page) ? n : sizeof(page) - 1] = '\0';

        response_ctx->status = HTTP_200_OK;
        response_ctx->body = (const uint8_t *)page;
        response_ctx->body_len = strlen(page);
        response_ctx->final_chunk = true;
        return 0;
    }
}

struct http_resource_detail_dynamic wifiPortalDetail = {
    .common = {
        .bitmask_of_supported_http_methods = BIT(HTTP_GET) | BIT(HTTP_POST),
        .type = HTTP_RESOURCE_TYPE_DYNAMIC,
    },
    .cb = wifiPortalHandler,
    .user_data = nullptr,
};
