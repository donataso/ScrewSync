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
#include "Nextion.hpp"
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

namespace
{
    struct SentAttr
    {
        char key[Nextion::KEY_MAX];
        char value[52];
        uint8_t page;
    };
    // Room for every page's attributes; global components keep their values across pages.
    constexpr size_t SENT_ATTR_MAX = 512;
    SentAttr g_sent[SENT_ATTR_MAX] __attribute__((section(".ext_ram.bss")));
    size_t g_sentCount = 0;
    uint8_t g_page = 0;

    SentAttr *findSent(const char *key)
    {
        for (size_t i = 0; i < g_sentCount; i++) {
            if (!strcmp(g_sent[i].key, key)) {
                return &g_sent[i];
            }
        }
        return nullptr;
    }
}

namespace Nextion
{
    bool pushAttr(const char *key, const char *value)
    {
        SentAttr *a = findSent(key);
        if (a && !strcmp(a->value, value)) {
            return false;
        }
        if (!a && g_sentCount < SENT_ATTR_MAX) {
            a = &g_sent[g_sentCount++];
            snprintf(a->key, sizeof(a->key), "%s", key);
            a->page = g_page;
        }
        if (a) {
            snprintf(a->value, sizeof(a->value), "%s", value);
        }
        char cmd[sizeof(SentAttr::key) + sizeof(SentAttr::value) + 2];
        snprintf(cmd, sizeof(cmd), "%s=%s", key, value);
        writeCommand(cmd);
        return true;
    }

    bool pushNum(const char *key, int value)
    {
        char v[16];
        snprintf(v, sizeof(v), "%d", value);
        return pushAttr(key, v);
    }

    bool pushText(const char *key, const char *fmt, ...)
    {
        char text[48];
        va_list args;
        va_start(args, fmt);
        vsnprintf(text, sizeof(text), fmt, args);
        va_end(args);
        for (char *s = text; *s; s++) {
            *s = safeChar(*s);
        }
        char v[sizeof(text) + 2];
        snprintf(v, sizeof(v), "\"%s\"", text);
        return pushAttr(key, v);
    }

    void pushColor(const char *component, const char *attr, int color)
    {
        char key[KEY_MAX];
        snprintf(key, sizeof(key), "%s.%s", component, attr);
        pushNum(key, color);
    }

    char safeChar(char c)
    {
        if (c == '"') {
            return '\'';
        }
        if (c == '\\') {
            return '/';
        }
        return c;
    }

    void setPage(uint8_t page)
    {
        g_page = page;
    }

    uint8_t page()
    {
        return g_page;
    }

    void resync()
    {
        g_sentCount = 0;
    }

    void forgetPage(uint8_t page)
    {
        size_t kept = 0;
        for (size_t i = 0; i < g_sentCount; i++) {
            if (g_sent[i].page != page) {
                g_sent[kept++] = g_sent[i];
            }
        }
        g_sentCount = kept;
    }
}
