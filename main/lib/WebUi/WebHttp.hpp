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
#ifndef WebHttp_h
#define WebHttp_h

#include <stddef.h>
#include <stdint.h>
#include <zephyr/net/http/server.h>
#include <zephyr/sys/util.h>

#ifdef CONFIG_ESP_SPIRAM
#define EXT_RAM_BSS __attribute__((section(".ext_ram.bss")))
#else
#define EXT_RAM_BSS
#endif

namespace WebHttp
{
    // Sets a final response; returns 0 for the resource callback.
    int reply(struct http_response_ctx *response, enum http_status status, const char *body);
    enum class ContentType
    {
        Text,
        Json,
    };
    int replyBody(struct http_response_ctx *response, enum http_status status, ContentType type, const void *body,
                  size_t len);

    // Replies 403 unless Machine::allowsUpdate(); true if it did.
    bool refuseUnlessUpdatable(struct http_response_ctx *response);

    const char *header(const struct http_request_ctx *request, const char *name);
    uint32_t contentLength(const struct http_request_ctx *request);

    // A query parameter's raw value into out; false if absent or too long.
    bool query(const struct http_client_ctx *client, const char *name, char *out, size_t outMax);
    float queryFloat(const struct http_client_ctx *client, const char *name, float fallback);

    // Appends JSON into a fixed buffer; ok turns false once it doesn't fit.
    struct JsonWriter
    {
        char *buf;
        size_t max;
        size_t pos = 0;
        bool ok = true;

        JsonWriter(char *b, size_t m) : buf(b), max(m) {}
        JsonWriter &raw(const char *s);
        JsonWriter &str(const char *s); // quoted and escaped
        JsonWriter &strOrNull(const char *s); // null when empty
        __attribute__((format(printf, 2, 3))) JsonWriter &fmt(const char *format, ...);
    };

    int replyJson(struct http_response_ctx *response, enum http_status status, const JsonWriter &json);
}

#endif
