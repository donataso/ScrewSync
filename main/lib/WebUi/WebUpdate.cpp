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
#include "WebRoutes.hpp"
#include "WebHttp.hpp"
#include <stdio.h>
#include <string.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/reboot.h>
#ifdef CONFIG_SCREWSYNC_DISPLAY_IMAGE
#include "DisplayImage.hpp"
#endif
#include "Ota.hpp"
#include "WsCommand.hpp"

LOG_MODULE_REGISTER(web_update, LOG_LEVEL_INF);

using WebHttp::reply;

namespace
{
#ifdef CONFIG_SCREWSYNC_DISPLAY_IMAGE
    constexpr size_t DISPLAY_IMAGE_MAX = 2 * 1024 * 1024;
#endif

    void rebootWork(struct k_work *)
    {
        sys_reboot(SYS_REBOOT_WARM);
    }
    K_WORK_DELAYABLE_DEFINE(g_rebootWork, rebootWork);

    struct Firmware
    {
        bool receiving = false;
        // The server may call again after the FINAL chunk was answered; ignored until the transaction ends.
        bool done = false;
        const struct http_client_ctx *owner = nullptr;

        void abort()
        {
            Ota::abort();
            receiving = false;
            owner = nullptr;
        }
    };
    Firmware g_fw;

    int handleUpdate(struct http_client_ctx *client, enum http_transaction_status status,
                     const struct http_request_ctx *request, struct http_response_ctx *response, void *)
    {
        if (g_fw.done) {
            if (status == HTTP_SERVER_TRANSACTION_ABORTED) {
                g_fw.done = false;
            }
            return 0;
        }
        if (status == HTTP_SERVER_TRANSACTION_ABORTED) {
            if (client == g_fw.owner) {
                g_fw.abort();
            }
            return 0;
        }
        if (g_fw.receiving && client != g_fw.owner) {
            return reply(response, HTTP_409_CONFLICT, "update already in progress\n");
        }
        // Every chunk: an upload is cut off if the machine starts running.
        if (WebHttp::refuseUnlessUpdatable(response)) {
            g_fw.abort();
            return 0;
        }
        if (!g_fw.receiving) {
            g_fw.receiving = true;
            g_fw.owner = client;
            WsCommand::closeAll();
            LOG_INF("ota: receiving firmware");
        }

        if (request->data_len && !Ota::writeChunk(request->data, request->data_len)) {
            g_fw.abort();
            if (status != HTTP_SERVER_REQUEST_DATA_FINAL) {
                return 0;
            }
            g_fw.done = true;
            return reply(response, HTTP_500_INTERNAL_SERVER_ERROR, "flash write failed\n");
        }
        if (status != HTTP_SERVER_REQUEST_DATA_FINAL) {
            return 0;
        }

        g_fw.receiving = false;
        g_fw.owner = nullptr;
        g_fw.done = true;
        LOG_INF("ota: firmware received, flashing");
        bool ok = Ota::finish();
        LOG_INF("ota: %s", ok ? "flashed, rebooting" : "flash failed");
        if (!ok) {
            return reply(response, HTTP_500_INTERNAL_SERVER_ERROR, "update failed\n");
        }
        k_work_schedule(&g_rebootWork, K_MSEC(300)); // lets the response flush first
        return reply(response, HTTP_200_OK, "ok, rebooting\n");
    }

#ifdef CONFIG_SCREWSYNC_DISPLAY_IMAGE
    uint8_t g_displayImage[DISPLAY_IMAGE_MAX] EXT_RAM_BSS;

    struct DisplayUpload
    {
        bool started = false;
        const struct http_client_ctx *owner = nullptr;
        uint32_t expected = 0; // headers only come with the first callback
        uint32_t received = 0;
        bool store = false;
    };
    DisplayUpload g_display;

    void readDisplayHeaders(const struct http_request_ctx *request)
    {
        if (!request->header_count) {
            return;
        }
        uint32_t len = WebHttp::contentLength(request);
        if (len) {
            g_display.expected = len;
        }
        const char *store = WebHttp::header(request, "X-Display-Store");
        if (store) {
            g_display.store = !strcmp(store, "1");
        }
    }

    // False once it has replied with the refusal.
    bool startDisplayUpload(struct http_client_ctx *client, struct http_response_ctx *response)
    {
        if (g_display.expected == 0 || g_display.expected > DISPLAY_IMAGE_MAX) {
            g_display = {};
            reply(response, HTTP_413_PAYLOAD_TOO_LARGE, "display image too large\n");
            return false;
        }
        const char *refusal = DisplayImage::beginFlash();
        if (refusal) {
            g_display = {};
            static char msg[48];
            snprintf(msg, sizeof(msg), "%s\n", refusal);
            reply(response, HTTP_409_CONFLICT, msg);
            return false;
        }
        g_display.started = true;
        g_display.owner = client;
        g_display.received = 0;
        WsCommand::closeAll();
        return true;
    }

    int handleUpdateDisplay(struct http_client_ctx *client, enum http_transaction_status status,
                            const struct http_request_ctx *request, struct http_response_ctx *response, void *)
    {
        if (status == HTTP_SERVER_TRANSACTION_ABORTED) {
            if (client == g_display.owner) {
                g_display = {};
                DisplayImage::endFlash();
            }
            return 0;
        }
        if (g_display.started && client != g_display.owner) {
            return reply(response, HTTP_409_CONFLICT, "update already in progress\n");
        }
        if (WebHttp::refuseUnlessUpdatable(response)) {
            if (g_display.started) {
                DisplayImage::endFlash();
            }
            g_display = {};
            return 0;
        }

        readDisplayHeaders(request);
        if (!g_display.started && !startDisplayUpload(client, response)) {
            return 0;
        }

        size_t space = DISPLAY_IMAGE_MAX - g_display.received;
        size_t n = MIN(request->data_len, space);
        memcpy(g_displayImage + g_display.received, request->data, n);
        g_display.received += n;
        if (status != HTTP_SERVER_REQUEST_DATA_FINAL) {
            return 0;
        }

        DisplayUpload upload = g_display;
        g_display = {};
        if (upload.received != upload.expected) {
            DisplayImage::endFlash();
            return reply(response, HTTP_400_BAD_REQUEST, "display image size mismatch\n");
        }

        LOG_INF("display ota: image received, flashing");
        const char *outcome;
        bool ok = DisplayImage::flash(g_displayImage, upload.received, upload.store, outcome);
        DisplayImage::endFlash();
        LOG_INF("display ota: %s", outcome);

        static char msg[64];
        snprintf(msg, sizeof(msg), "%s\n", outcome);
        return reply(response, ok ? HTTP_200_OK : HTTP_500_INTERNAL_SERVER_ERROR, msg);
    }
#endif
}

namespace WebRoutes
{
    struct http_resource_detail_dynamic g_update = {
        .common = {.bitmask_of_supported_http_methods = BIT(HTTP_POST), .type = HTTP_RESOURCE_TYPE_DYNAMIC},
        .cb = handleUpdate,
        .user_data = nullptr,
    };

#ifdef CONFIG_SCREWSYNC_DISPLAY_IMAGE
    struct http_resource_detail_dynamic g_updateDisplay = {
        .common = {.bitmask_of_supported_http_methods = BIT(HTTP_POST), .type = HTTP_RESOURCE_TYPE_DYNAMIC},
        .cb = handleUpdateDisplay,
        .user_data = nullptr,
    };
#endif
}
