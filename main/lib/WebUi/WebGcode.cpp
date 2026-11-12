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
#include <string.h>
#include "GcodeStore.hpp"

using WebHttp::JsonWriter;
using WebHttp::reply;

namespace
{
    constexpr size_t REPLY_MAX = 1024;
    char g_reply[REPLY_MAX] EXT_RAM_BSS;

    constexpr size_t LIST_JSON_MAX = 16 * 1024;
    char g_listJson[LIST_JSON_MAX] EXT_RAM_BSS;
    GcodeStore::ListResult g_list EXT_RAM_BSS; // too big for the stack

    uint8_t g_upload[GcodeStore::ENCODED_MAX] EXT_RAM_BSS;
    struct Upload
    {
        const struct http_client_ctx *owner = nullptr;
        uint32_t expected = 0;
        uint32_t received = 0;
    };
    Upload g_uploadState;

    enum class Target
    {
        Save,
        Run,
        Update,
    };
    const Target SAVE = Target::Save;
    const Target RUN = Target::Run;
    const Target UPDATE = Target::Update;

    void writeActive(JsonWriter &j)
    {
        GcodeStore::ActiveInfo a = GcodeStore::active();
        if (!a.present)
        {
            j.raw("null");
            return;
        }
        j.raw("{\"name\":").str(a.name).raw(",\"flavor\":").str(a.flavor);
        j.fmt(",\"size\":%zu,\"steps\":%u,\"id\":", a.size, (unsigned)a.steps).strOrNull(a.savedId);
        j.fmt(",\"version\":%u}", (unsigned)a.version);
    }

    int replyResult(struct http_response_ctx *response, const GcodeStore::Result &r)
    {
        JsonWriter j(g_reply, REPLY_MAX);
        if (r.ok)
        {
            j.raw("{\"ok\":true,\"id\":").strOrNull(r.savedId).raw(",\"active\":");
            writeActive(j);
            j.raw("}");
        }
        else
        {
            j.raw("{\"ok\":false,\"error\":").str(r.error ? r.error : "error").fmt(",\"line\":%u}", (unsigned)r.line);
        }
        return WebHttp::replyJson(response, r.ok ? HTTP_200_OK : HTTP_400_BAD_REQUEST, j);
    }

    int replyDeleted(struct http_response_ctx *response, const char *error)
    {
        JsonWriter j(g_reply, REPLY_MAX);
        if (error) {
            j.raw("{\"ok\":false,\"error\":").str(error).raw("}");
        } else {
            j.raw("{\"ok\":true}");
        }
        return WebHttp::replyJson(response, error ? HTTP_400_BAD_REQUEST : HTTP_200_OK, j);
    }

    void queryId(const struct http_client_ctx *client, char (&id)[GcodeStore::ID_LEN + 1])
    {
        WebHttp::query(client, "id", id, sizeof(id));
    }

    int replyText(struct http_response_ctx *response, const char *text, size_t len)
    {
        return WebHttp::replyBody(response, HTTP_200_OK, WebHttp::ContentType::Text, text, len);
    }

    int handleList(struct http_client_ctx *, enum http_transaction_status status, const struct http_request_ctx *,
                   struct http_response_ctx *response, void *)
    {
        if (status != HTTP_SERVER_REQUEST_DATA_FINAL) {
            return 0;
        }
        if (!GcodeStore::list(g_list)) {
            return reply(response, HTTP_500_INTERNAL_SERVER_ERROR, "filesystem error\n");
        }
        JsonWriter j(g_listJson, LIST_JSON_MAX);
        j.raw("{\"programs\":[");
        for (size_t i = 0; j.ok && i < g_list.count; i++)
        {
            const GcodeStore::ProgramInfo &p = g_list.programs[i];
            j.raw(i ? ",{\"id\":" : "{\"id\":").str(p.id).raw(",\"name\":").str(p.name);
            j.raw(",\"flavor\":").str(p.flavor).fmt(",\"size\":%zu}", p.size);
        }
        j.fmt("],\"free\":%llu,\"active\":", (unsigned long long)g_list.freeBytes);
        writeActive(j);
        j.raw("}");
        return WebHttp::replyJson(response, HTTP_200_OK, j);
    }

    int handleUpload(struct http_client_ctx *client, enum http_transaction_status status,
                     const struct http_request_ctx *request, struct http_response_ctx *response, void *user_data)
    {
        Upload &u = g_uploadState;
        if (status == HTTP_SERVER_TRANSACTION_ABORTED)
        {
            if (client == u.owner) {
                u = {};
            }
            return 0;
        }
        if (WebHttp::refuseUnlessUpdatable(response)) {
            return 0;
        }
        if (u.owner && client != u.owner) {
            return reply(response, HTTP_409_CONFLICT, "upload in progress\n");
        }
        if (!u.owner)
        {
            uint32_t len = request->header_count ? WebHttp::contentLength(request) : 0;
            if (len == 0 || len > GcodeStore::ENCODED_MAX) {
                return reply(response, HTTP_413_PAYLOAD_TOO_LARGE, "program too large\n");
            }
            u.owner = client;
            u.expected = len;
            u.received = 0;
        }

        size_t n = MIN(request->data_len, u.expected - u.received);
        memcpy(g_upload + u.received, request->data, n);
        u.received += n;
        if (status != HTTP_SERVER_REQUEST_DATA_FINAL) {
            return 0;
        }

        Upload done = u;
        u = {};
        if (done.received != done.expected) {
            return reply(response, HTTP_400_BAD_REQUEST, "size mismatch\n");
        }
        const char *body = (const char *)g_upload;
        Target target = *(const Target *)user_data;
        if (target == Target::Update)
        {
            char id[GcodeStore::ID_LEN + 1];
            queryId(client, id);
            return replyResult(response, GcodeStore::updateEncoded(id, body, done.received));
        }
        bool run = target == Target::Run;
        return replyResult(response, GcodeStore::uploadEncoded(body, done.received, !run, run));
    }

    int handleActive(struct http_client_ctx *, enum http_transaction_status status, const struct http_request_ctx *,
                     struct http_response_ctx *response, void *)
    {
        if (status != HTTP_SERVER_REQUEST_DATA_FINAL) {
            return 0;
        }
        size_t len;
        const char *text = GcodeStore::activeText(len);
        if (!text) {
            return reply(response, HTTP_404_NOT_FOUND, "no active program\n");
        }
        return replyText(response, text, len);
    }

    int handleProgram(struct http_client_ctx *client, enum http_transaction_status status,
                      const struct http_request_ctx *, struct http_response_ctx *response, void *)
    {
        if (status != HTTP_SERVER_REQUEST_DATA_FINAL) {
            return 0;
        }
        char id[GcodeStore::ID_LEN + 1];
        queryId(client, id);
        const char *text;
        size_t len;
        const char *error = GcodeStore::read(id, text, len);
        if (error) {
            return reply(response, HTTP_404_NOT_FOUND, error);
        }
        return replyText(response, text, len);
    }

    int handleSelect(struct http_client_ctx *client, enum http_transaction_status status,
                     const struct http_request_ctx *, struct http_response_ctx *response, void *)
    {
        if (status != HTTP_SERVER_REQUEST_DATA_FINAL || WebHttp::refuseUnlessUpdatable(response)) {
            return 0;
        }
        char id[GcodeStore::ID_LEN + 1];
        queryId(client, id);
        return replyResult(response, GcodeStore::select(id));
    }

    int handleDelete(struct http_client_ctx *client, enum http_transaction_status status,
                     const struct http_request_ctx *, struct http_response_ctx *response, void *)
    {
        if (status != HTTP_SERVER_REQUEST_DATA_FINAL || WebHttp::refuseUnlessUpdatable(response)) {
            return 0;
        }
        char id[GcodeStore::ID_LEN + 1];
        queryId(client, id);
        return replyDeleted(response, GcodeStore::remove(id));
    }
}

#define GCODE_ROUTE(name, method, handler, data)                                                                   \
    struct http_resource_detail_dynamic name = {                                                                   \
        .common = {.bitmask_of_supported_http_methods = BIT(method), .type = HTTP_RESOURCE_TYPE_DYNAMIC},          \
        .cb = handler, .user_data = (void *)(data),                                                                \
    }

namespace WebRoutes
{
    GCODE_ROUTE(gcodeList, HTTP_GET, handleList, nullptr);
    GCODE_ROUTE(gcodeSave, HTTP_POST, handleUpload, &SAVE);
    GCODE_ROUTE(gcodeUpdate, HTTP_POST, handleUpload, &UPDATE);
    GCODE_ROUTE(gcodeRun, HTTP_POST, handleUpload, &RUN);
    GCODE_ROUTE(gcodeActive, HTTP_GET, handleActive, nullptr);
    GCODE_ROUTE(gcodeProgram, HTTP_GET, handleProgram, nullptr);
    GCODE_ROUTE(gcodeSelect, HTTP_POST, handleSelect, nullptr);
    GCODE_ROUTE(gcodeDelete, HTTP_POST, handleDelete, nullptr);
}
