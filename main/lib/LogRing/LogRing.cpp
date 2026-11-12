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
#include "LogRing.hpp"
#include <string.h>
#include <zephyr/kernel.h>
#include <zephyr/data/json.h>
#include <zephyr/logging/log_backend.h>
#include <zephyr/logging/log_output.h>
#include <zephyr/sys/util.h>

#ifdef CONFIG_ESP_SPIRAM
#define EXT_RAM_BSS __attribute__((section(".ext_ram.bss")))
#else
#define EXT_RAM_BSS
#endif

namespace
{
    constexpr size_t LINES_MAX = 200;
    constexpr size_t LINE_LEN_MAX = 256;
    constexpr uint32_t OUTPUT_FLAGS = LOG_OUTPUT_FLAG_LEVEL | LOG_OUTPUT_FLAG_TIMESTAMP |
                                      LOG_OUTPUT_FLAG_FORMAT_TIMESTAMP | LOG_OUTPUT_FLAG_CRLF_NONE;

    char g_ring[LINES_MAX][LINE_LEN_MAX + 1] EXT_RAM_BSS;
    uint32_t g_written = 0;
    size_t g_lineLen = 0;
    struct k_spinlock g_lock;

    char g_snapshot[LINES_MAX][LINE_LEN_MAX + 1] EXT_RAM_BSS;

    struct Lines
    {
        const char *lines[LINES_MAX];
        size_t count;
    };
    Lines g_lines;

    const struct json_obj_descr LINES_DESCR[] = {
        JSON_OBJ_DESCR_ARRAY(Lines, lines, LINES_MAX, count, JSON_TOK_STRING),
    };

    int appendChunk(uint8_t *data, size_t len, void *)
    {
        size_t n = MIN(len, LINE_LEN_MAX - g_lineLen);
        memcpy(g_ring[g_written % LINES_MAX] + g_lineLen, data, n);
        g_lineLen += n;
        return (int)len;
    }

    uint8_t g_outputBuf[LINE_LEN_MAX];
    LOG_OUTPUT_DEFINE(OUTPUT, appendChunk, g_outputBuf, sizeof(g_outputBuf));

    void process(const struct log_backend *const, union log_msg_generic *msg)
    {
        k_spinlock_key_t key = k_spin_lock(&g_lock);
        g_lineLen = 0;
        log_output_msg_process(&OUTPUT, &msg->log, OUTPUT_FLAGS);
        g_ring[g_written % LINES_MAX][g_lineLen] = '\0';
        g_written++;
        k_spin_unlock(&g_lock, key);
    }

    void panic(const struct log_backend *const)
    {}

    const struct log_backend_api BACKEND_API = {
        .process = process,
        .panic = panic,
    };
}

LOG_BACKEND_DEFINE(LOG_RING_BACKEND, BACKEND_API, true);

int LogRing::toJson(char *out, size_t outMax)
{
    k_spinlock_key_t key = k_spin_lock(&g_lock);
    uint32_t end = g_written;
    k_spin_unlock(&g_lock, key);

    // One line per lock hold keeps interrupt-off time short.
    g_lines.count = 0;
    for (uint32_t i = end > LINES_MAX ? end - LINES_MAX : 0; i < end; i++) {
        key = k_spin_lock(&g_lock);
        bool stillBuffered = g_written - i <= LINES_MAX;
        if (stillBuffered) {
            memcpy(g_snapshot[g_lines.count], g_ring[i % LINES_MAX], LINE_LEN_MAX + 1);
        }
        k_spin_unlock(&g_lock, key);
        if (stillBuffered) {
            g_lines.lines[g_lines.count] = g_snapshot[g_lines.count];
            g_lines.count++;
        }
    }

    int rc = json_arr_encode_buf(LINES_DESCR, &g_lines, out, outMax);
    return rc == 0 ? (int)strlen(out) : -1;
}
