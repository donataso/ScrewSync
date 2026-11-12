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

HTTP_SERVER_REGISTER_HEADER_CAPTURE(IF_NONE_MATCH_CAPTURE, "If-None-Match");

namespace
{
    constexpr char ETAG[] = "\"" FW_VERSION "\"";

    int handleIndex(struct http_client_ctx *, enum http_transaction_status status,
                    const struct http_request_ctx *requestCtx, struct http_response_ctx *responseCtx, void *)
    {
        if (status != HTTP_SERVER_REQUEST_DATA_FINAL) {
            return 0;
        }

        if (Machine::blocksTraffic()) {
            static char msg[32];
            snprintf(msg, sizeof(msg), "%s\n", Machine::stateName());
            responseCtx->status = HTTP_503_SERVICE_UNAVAILABLE;
            responseCtx->body = (const uint8_t *)msg;
            responseCtx->body_len = strlen(msg);
            responseCtx->final_chunk = true;
            return 0;
        }

        for (size_t i = 0; i < requestCtx->header_count; i++) {
            if (strcasecmp(requestCtx->headers[i].name, "If-None-Match") == 0 &&
                strcmp(requestCtx->headers[i].value, ETAG) == 0) {
                static const struct http_header CACHE_HEADER[] = {{"Cache-Control", "max-age=31536000"}};
                responseCtx->status = HTTP_304_NOT_MODIFIED;
                responseCtx->headers = CACHE_HEADER;
                responseCtx->header_count = 1;
                responseCtx->body = nullptr;
                responseCtx->body_len = 0;
                responseCtx->final_chunk = true;
                return 0;
            }
        }

        static const struct http_header GZ_HEADER[] = {
            {"Content-Encoding", "gzip"},
            {"ETag", ETAG},
            {"Cache-Control", "max-age=31536000"},
        };
        responseCtx->headers = GZ_HEADER;
        responseCtx->header_count = 3;
        responseCtx->status = HTTP_200_OK;
        responseCtx->body = WEB_INDEX_HTML_GZ;
        responseCtx->body_len = WEB_INDEX_HTML_GZ_LEN;
        responseCtx->final_chunk = true;
        return 0;
    }
}

struct http_resource_detail_dynamic g_indexDetail = {
    .common = {.bitmask_of_supported_http_methods = BIT(HTTP_GET), .type = HTTP_RESOURCE_TYPE_DYNAMIC},
    .cb = handleIndex,
    .user_data = nullptr,
};
