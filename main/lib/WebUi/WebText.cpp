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
#include "WebText.hpp"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

namespace
{
    const char *escape(char c, char (&buf)[8])
    {
        switch (c) {
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
            if ((unsigned char)c < 0x20) {
                snprintf(buf, sizeof(buf), "\\u%04x", (unsigned char)c);
                return buf;
            }
            return nullptr;
        }
    }
}

namespace WebHttp
{
    const char *urlQueryValue(const char *url, const char *name)
    {
        size_t nameLen = strlen(name);
        const char *q = strchr(url, '?');
        while (q) {
            q++;
            if (!strncmp(q, name, nameLen) && q[nameLen] == '=') {
                return q + nameLen + 1;
            }
            q = strchr(q, '&');
        }
        return nullptr;
    }

    bool urlQuery(const char *url, const char *name, char *out, size_t outMax)
    {
        out[0] = '\0';
        const char *v = urlQueryValue(url, name);
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
        for (; *s && ok; s++) {
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
