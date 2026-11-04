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
HTTP_SERVER_REGISTER_HEADER_CAPTURE(g_contentLengthCapture, "Content-Length");
HTTP_SERVER_REGISTER_HEADER_CAPTURE(g_displayStoreCapture, "X-Display-Store");

namespace
{
    // Value of name in the URL's query string, or nullptr; ends at '&' or NUL.
    const char *queryValue(const struct http_client_ctx *client, const char *name)
    {
        size_t nameLen = strlen(name);
        const char *q = strchr((const char *)client->url_buffer, '?');
        while (q)
        {
            q++;
            if (!strncmp(q, name, nameLen) && q[nameLen] == '=') {
                return q + nameLen + 1;
            }
            q = strchr(q, '&');
        }
        return nullptr;
    }

    const char *escape(char c, char (&buf)[8])
    {
        switch (c)
        {
        case '"':
            return "\\\"";
        case '\\':
            return "\\\\";
        case '\n':
            return "\\n";
        case '\r':
            return "\\r";
        case '\t':
            return "\\t";
        default:
            if ((unsigned char)c < 0x20)
            {
                snprintf(buf, sizeof(buf), "\\u%04x", (unsigned char)c);
                return buf;
            }
            return nullptr;
        }
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
        out[0] = '\0';
        const char *v = queryValue(client, name);
        if (!v) {
            return false;
        }
        size_t n = strcspn(v, "&");
        if (n >= outMax) {
            return false;
        }
        memcpy(out, v, n);
        out[n] = '\0';
        return true;
    }

    float queryFloat(const struct http_client_ctx *client, const char *name, float fallback)
    {
        const char *v = queryValue(client, name);
        return v ? strtof(v, nullptr) : fallback;
    }

    JsonWriter &JsonWriter::raw(const char *s)
    {
        size_t n = strlen(s);
        if (!ok || pos + n >= max) {
            ok = false;
            return *this;
        }
        memcpy(buf + pos, s, n);
        pos += n;
        return *this;
    }

    JsonWriter &JsonWriter::str(const char *s)
    {
        raw("\"");
        for (; *s && ok; s++)
        {
            char esc[8];
            const char *e = escape(*s, esc);
            if (e) {
                raw(e);
            } else if (pos + 1 < max) {
                buf[pos++] = *s;
            } else {
                ok = false;
            }
        }
        return raw("\"");
    }

    JsonWriter &JsonWriter::strOrNull(const char *s)
    {
        return *s ? str(s) : raw("null");
    }

    JsonWriter &JsonWriter::fmt(const char *format, ...)
    {
        char tmp[64];
        va_list args;
        va_start(args, format);
        int n = vsnprintf(tmp, sizeof(tmp), format, args);
        va_end(args);
        if (n < 0 || (size_t)n >= sizeof(tmp)) {
            ok = false;
            return *this;
        }
        return raw(tmp);
    }
}
