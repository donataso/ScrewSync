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
#include "Backup.hpp"
#include "Display.hpp"
#include "LogRing.hpp"
#include "Machine.hpp"
#include "ThreadStore.hpp"

using WebHttp::reply;

namespace
{
    constexpr size_t PREVIEW_MAX = 64 * 1024; // display/app.toml memory_file_storage_size
    uint8_t g_preview[PREVIEW_MAX] EXT_RAM_BSS;

    struct PreviewUpload
    {
        const struct http_client_ctx *owner = nullptr;
        uint32_t expected = 0;
        uint32_t received = 0;
        Display::PreviewMapping mapping;
    };
    PreviewUpload g_previewUpload;

    int handlePreview(struct http_client_ctx *client, enum http_transaction_status status,
                      const struct http_request_ctx *request, struct http_response_ctx *response, void *)
    {
        PreviewUpload &u = g_previewUpload;
        if (status == HTTP_SERVER_TRANSACTION_ABORTED)
        {
            if (client == u.owner) {
                u.owner = nullptr;
            }
            return 0;
        }
        if (u.owner && client != u.owner) {
            return reply(response, HTTP_409_CONFLICT, "preview upload in progress\n");
        }
        if (!u.owner)
        {
            if (WebHttp::refuseUnlessUpdatable(response)) {
                return 0;
            }
            uint32_t len = request->header_count ? WebHttp::contentLength(request) : 0;
            if (len == 0 || len > PREVIEW_MAX) {
                return reply(response, HTTP_413_PAYLOAD_TOO_LARGE, "preview too large\n");
            }
            u.owner = client;
            u.expected = len;
            u.received = 0;
            u.mapping = {WebHttp::queryFloat(client, "ox", 0), WebHttp::queryFloat(client, "oy", 0),
                         WebHttp::queryFloat(client, "s", 0)};
        }

        size_t n = MIN(request->data_len, u.expected - u.received);
        memcpy(g_preview + u.received, request->data, n);
        u.received += n;
        if (status != HTTP_SERVER_REQUEST_DATA_FINAL) {
            return 0;
        }
        u.owner = nullptr;
        if (u.received != u.expected) {
            return reply(response, HTTP_400_BAD_REQUEST, "preview size mismatch\n");
        }
        if (!Display::sendPreview(g_preview, u.received, u.mapping)) {
            return reply(response, HTTP_500_INTERNAL_SERVER_ERROR, "preview transfer failed\n");
        }
        return reply(response, HTTP_200_OK, "ok\n");
    }

    constexpr size_t JSON_DOC_MAX = 128 * 1024;
    char g_jsonDoc[JSON_DOC_MAX] EXT_RAM_BSS;

    struct JsonUpload
    {
        const struct http_client_ctx *owner = nullptr;
        size_t len = 0;
        bool overflow = false;
    };
    JsonUpload g_jsonUpload;

    struct JsonDocument
    {
        int (*toJson)(char *out, size_t outMax);
        const char *(*fromJson)(char *json, size_t len);
    };

    int replaceDoc(const JsonDocument &doc, const JsonUpload &upload, struct http_response_ctx *response)
    {
        if (upload.overflow) {
            return reply(response, HTTP_413_PAYLOAD_TOO_LARGE, "too large\n");
        }
        g_jsonDoc[upload.len] = '\0';
        static char msg[80];
        const char *refusal = doc.fromJson(g_jsonDoc, upload.len);
        snprintf(msg, sizeof(msg), "%s\n", refusal ? refusal : "ok");
        return reply(response, refusal ? HTTP_400_BAD_REQUEST : HTTP_200_OK, msg);
    }

    // GET: the document as JSON. POST: replaces it.
    int handleJsonDoc(struct http_client_ctx *client, enum http_transaction_status status,
                      const struct http_request_ctx *request, struct http_response_ctx *response, void *user_data)
    {
        const JsonDocument *doc = (const JsonDocument *)user_data;
        JsonUpload &u = g_jsonUpload;
        if (status == HTTP_SERVER_TRANSACTION_ABORTED || status == HTTP_SERVER_TRANSACTION_COMPLETE)
        {
            if (client == u.owner) {
                u = {};
            }
            return 0;
        }
        if (u.owner && client != u.owner) {
            return reply(response, HTTP_409_CONFLICT, "busy\n");
        }
        u.owner = client;
        if (Machine::blocksTraffic())
        {
            u = {};
            return reply(response, HTTP_503_SERVICE_UNAVAILABLE, "busy\n");
        }

        if (client->method == HTTP_POST && request->data_len)
        {
            if (u.len + request->data_len < JSON_DOC_MAX) {
                memcpy(g_jsonDoc + u.len, request->data, request->data_len);
                u.len += request->data_len;
            } else {
                u.overflow = true;
            }
        }
        if (status != HTTP_SERVER_REQUEST_DATA_FINAL) {
            return 0;
        }

        JsonUpload upload = u;
        u = {};
        if (client->method == HTTP_POST) {
            return replaceDoc(*doc, upload, response);
        }
        int len = doc->toJson(g_jsonDoc, JSON_DOC_MAX);
        if (len < 0) {
            return reply(response, HTTP_500_INTERNAL_SERVER_ERROR, "encode failed\n");
        }
        return WebHttp::replyBody(response, HTTP_200_OK, WebHttp::ContentType::Json, g_jsonDoc, (size_t)len);
    }

    const JsonDocument threadsDoc = {ThreadStore::toJson, ThreadStore::fromJson};
    const JsonDocument backupDoc = {Backup::toJson, Backup::fromJson};
    const JsonDocument logDoc = {LogRing::toJson, nullptr};
}

namespace WebRoutes
{
    struct http_resource_detail_dynamic preview = {
        .common = {.bitmask_of_supported_http_methods = BIT(HTTP_POST), .type = HTTP_RESOURCE_TYPE_DYNAMIC},
        .cb = handlePreview, .user_data = nullptr,
    };

    struct http_resource_detail_dynamic threads = {
        .common = {.bitmask_of_supported_http_methods = BIT(HTTP_GET) | BIT(HTTP_POST),
                   .type = HTTP_RESOURCE_TYPE_DYNAMIC},
        .cb = handleJsonDoc, .user_data = (void *)&threadsDoc,
    };

    struct http_resource_detail_dynamic backup = {
        .common = {.bitmask_of_supported_http_methods = BIT(HTTP_GET) | BIT(HTTP_POST),
                   .type = HTTP_RESOURCE_TYPE_DYNAMIC},
        .cb = handleJsonDoc, .user_data = (void *)&backupDoc,
    };

    struct http_resource_detail_dynamic logRing = {
        .common = {.bitmask_of_supported_http_methods = BIT(HTTP_GET), .type = HTTP_RESOURCE_TYPE_DYNAMIC},
        .cb = handleJsonDoc, .user_data = (void *)&logDoc,
    };
}
