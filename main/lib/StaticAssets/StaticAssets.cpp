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
#include "StaticAssets.hpp"
#include "web_assets.hpp"
#include "Machine.hpp"
#include <stdio.h>
#include <string.h>
#include <strings.h>

#ifndef FW_VERSION
#define FW_VERSION "dev"
#endif

HTTP_SERVER_REGISTER_HEADER_CAPTURE(g_ifNoneMatchCapture, "If-None-Match");

namespace
{
    constexpr char ETAG[] = "\"" FW_VERSION "\"";

    int handleIndex(struct http_client_ctx *, enum http_transaction_status status,
                     const struct http_request_ctx *request_ctx, struct http_response_ctx *response_ctx, void *)
    {
        if (status != HTTP_SERVER_REQUEST_DATA_FINAL) {
            return 0;
        }

        if (Machine::blocksTraffic())
        {
            static char msg[32];
            snprintf(msg, sizeof(msg), "%s\n", Machine::stateName());
            response_ctx->status = HTTP_503_SERVICE_UNAVAILABLE;
            response_ctx->body = (const uint8_t *)msg;
            response_ctx->body_len = strlen(msg);
            response_ctx->final_chunk = true;
            return 0;
        }

        for (size_t i = 0; i < request_ctx->header_count; i++)
        {
            if (strcasecmp(request_ctx->headers[i].name, "If-None-Match") == 0 &&
                strcmp(request_ctx->headers[i].value, ETAG) == 0)
            {
                static const struct http_header cacheHeader[] = {{"Cache-Control", "max-age=31536000"}};
                response_ctx->status = HTTP_304_NOT_MODIFIED;
                response_ctx->headers = cacheHeader;
                response_ctx->header_count = 1;
                response_ctx->body = nullptr;
                response_ctx->body_len = 0;
                response_ctx->final_chunk = true;
                return 0;
            }
        }

        static const struct http_header gzHeader[] = {
            {"Content-Encoding", "gzip"},
            {"ETag", ETAG},
            {"Cache-Control", "max-age=31536000"},
        };
        response_ctx->headers = gzHeader;
        response_ctx->header_count = 3;
        response_ctx->status = HTTP_200_OK;
        response_ctx->body = WEB_INDEX_HTML_GZ;
        response_ctx->body_len = WEB_INDEX_HTML_GZ_LEN;
        response_ctx->final_chunk = true;
        return 0;
    }
}

struct http_resource_detail_dynamic indexDetail = {
    .common = {.bitmask_of_supported_http_methods = BIT(HTTP_GET), .type = HTTP_RESOURCE_TYPE_DYNAMIC},
    .cb = handleIndex,
    .user_data = nullptr,
};
