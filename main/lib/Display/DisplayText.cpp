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
#include "DisplayText.hpp"

#include "ThreadTable.hpp"

#include <ctype.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

using ModeCells::Cell;
using ModeCells::Kind;

namespace Display::Detail
{
    const char *unitSuffix(bool inch)
    {
        return inch ? "in" : "mm";
    }

    void formatLength(double du, bool inch, char *out, size_t outMax)
    {
        long long v = llround(inch ? du / 25.4 : du / 10.0);
        long long scale = inch ? 10000 : 1000;
        const char *sign = v < 0 ? "-" : "";
        long long a = llabs(v);
        if (inch) {
            snprintf(out, outMax, "%s%lld.%04lld", sign, a / scale, a % scale);
        } else {
            snprintf(out, outMax, "%s%lld.%03lld", sign, a / scale, a % scale);
        }
    }

    void trimZeros(char *s)
    {
        if (!strchr(s, '.')) {
            return;
        }
        size_t n = strlen(s);
        while (n > 0 && s[n - 1] == '0') {
            s[--n] = '\0';
        }
        if (n > 0 && s[n - 1] == '.') {
            s[n - 1] = '\0';
        }
    }

    bool paramValue(const char *params, const char *name, char *out, size_t outMax)
    {
        size_t len = strlen(name);
        for (const char *p = params; p && *p; p = strchr(p, ' ')) {
            p += *p == ' ';
            if (!strncmp(p, name, len) && p[len] == '=') {
                const char *v = p + len + 1;
                snprintf(out, outMax, "%.*s", (int)strcspn(v, " "), v);
                return true;
            }
        }
        return false;
    }

    bool cellShown(const Cell &c, const char *params)
    {
        if (!c.when) {
            return true;
        }
        const char *eq = strchr(c.when, '=');
        char name[16];
        char v[16];
        snprintf(name, sizeof(name), "%.*s", (int)(eq - c.when), c.when);
        return paramValue(params, name, v, sizeof(v)) && !strcmp(v, eq + 1);
    }

    bool formatCellValue(const Cell &c, const char *raw, bool inch, char *out, size_t outMax)
    {
        if (c.kind == Kind::Count) {
            snprintf(out, outMax, "%s", raw);
            return true;
        }
        if (c.kind == Kind::Angle) {
            snprintf(out, outMax, "%.2f", strtod(raw, nullptr));
            trimZeros(out);
            if (!strcmp(out, "-0")) {
                strcpy(out, "0");
            }
            snprintf(out + strlen(out), outMax - strlen(out), "°");
            return true;
        }
        long du = strtol(raw, nullptr, 10);
        char length[24];
        formatLength((double)du, inch, length, sizeof(length));
        trimZeros(length);
        const char *per;
        if (c.kind == Kind::Speed) {
            per = "/min";
        } else if (c.kind == Kind::Feed) {
            per = "/rev";
        } else {
            per = "";
        }
        snprintf(out, outMax, "%s %s%s", length, unitSuffix(inch), per);
        return du != 0;
    }

    void groupLabel(const char *name, char *out, size_t outMax)
    {
        size_t len = strlen(name);
        size_t i = 0;
        for (; i < len && i + 1 < outMax; i++) {
            out[i] = len <= 4 || i == 0 ? (char)toupper((unsigned char)name[i]) : name[i];
        }
        out[i] = '\0';
    }

    void designationLabel(const char *designation, char *out, size_t outMax)
    {
        size_t n = 0;
        for (const char *p = designation; *p && n + 3 < outMax; p++) {
            bool times = *p == 'x' && p > designation && isdigit((unsigned char)p[-1]) && isdigit((unsigned char)p[1]);
            if (times) {
                out[n++] = '\xC3';
                out[n++] = '\x97';
            } else {
                out[n++] = *p;
            }
        }
        out[n] = '\0';
    }

    bool nextItem(const char *&list, char *out, size_t outMax)
    {
        if (!*list) {
            out[0] = '\0';
            return false;
        }
        size_t len = strcspn(list, ",");
        snprintf(out, outMax, "%.*s", (int)len, list);
        list += len + (list[len] == ',');
        return true;
    }

    bool hasItem(const char *list, const char *name)
    {
        char item[ThreadTable::DESIGNATION_MAX];
        while (nextItem(list, item, sizeof(item))) {
            if (!strcmp(item, name)) {
                return true;
            }
        }
        return false;
    }
}
