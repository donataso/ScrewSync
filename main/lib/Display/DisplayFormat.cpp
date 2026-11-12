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
#include "DisplayInternal.hpp"
#include "RunSettings.hpp"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

namespace Display::Detail
{
    bool inchUnits()
    {
        return RunSettings::units() == RunSettings::Units::Inch;
    }

    const char *unitSuffix()
    {
        return inchUnits() ? "in" : "mm";
    }

    void formatLength(double du, char *out, size_t outMax)
    {
        bool inch = inchUnits();
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
}
