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
#include "WebHttp.hpp"
#include "Machine.hpp"
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

// Needed so request headers carry them: only registered headers are captured.
HTTP_SERVER_REGISTER_HEADER_CAPTURE(CONTENT_LENGTH_CAPTURE, "Content-Length");
#ifdef CONFIG_SCREWSYNC_DISPLAY_IMAGE
HTTP_SERVER_REGISTER_HEADER_CAPTURE(DISPLAY_STORE_CAPTURE, "X-Display-Store");
#endif

namespace
{
    const char *queryValue(const struct http_client_ctx *client, const char *name)
    {
        return WebHttp::urlQueryValue((const char *)client->url_buffer, name);
    }
}

namespace WebHttp
{
    int reply(struct http_response_ctx *response, enum http_status status, const char *body)
    {
        response->status = status;
        response->body = (const uint8_t *)body;
        response->body_len = strlen(body);
        response->final_chunk = true;
        return 0;
    }

    int replyBody(struct http_response_ctx *response, enum http_status status, ContentType type, const void *body,
                  size_t len)
    {
        static const struct http_header JSON[] = {{"Content-Type", "application/json"}};
        static const struct http_header TEXT[] = {{"Content-Type", "text/plain"}};
        response->status = status;
        response->headers = type == ContentType::Json ? JSON : TEXT;
        response->header_count = 1;
        response->body = (const uint8_t *)body;
        response->body_len = len;
        response->final_chunk = true;
        return 0;
    }

    int replyJson(struct http_response_ctx *response, enum http_status status, const JsonWriter &json)
    {
        if (!json.ok) {
            return reply(response, HTTP_500_INTERNAL_SERVER_ERROR, "response too large\n");
        }
        return replyBody(response, status, ContentType::Json, json.buf, json.pos);
    }

    bool refuseUnlessUpdatable(struct http_response_ctx *response)
    {
        if (Machine::allowsUpdate()) {
            return false;
        }
        reply(response, HTTP_403_FORBIDDEN, "update blocked: machine not idle\n");
        return true;
    }

    const char *header(const struct http_request_ctx *request, const char *name)
    {
        for (size_t i = 0; i < request->header_count; i++) {
            if (strcasecmp(request->headers[i].name, name) == 0) {
                return request->headers[i].value;
            }
        }
        return nullptr;
    }

    uint32_t contentLength(const struct http_request_ctx *request)
    {
        const char *value = header(request, "Content-Length");
        return value ? (uint32_t)strtoul(value, nullptr, 10) : 0;
    }

    bool query(const struct http_client_ctx *client, const char *name, char *out, size_t outMax)
    {
        return urlQuery((const char *)client->url_buffer, name, out, outMax);
    }

    float queryFloat(const struct http_client_ctx *client, const char *name, float fallback)
    {
        const char *v = queryValue(client, name);
        return v ? strtof(v, nullptr) : fallback;
    }
}
