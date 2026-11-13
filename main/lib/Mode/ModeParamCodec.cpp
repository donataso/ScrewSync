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
#include "ModeParamCodec.hpp"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

namespace
{
    using Mode::Detail::Kind;
    using Mode::Detail::Param;

    constexpr double RAD_PER_DEG = 3.14159265358979323846 / 180;
    constexpr size_t TOKEN_MAX = 64;

    void flagName(const Param &p, bool value, char *out, size_t outMax)
    {
        const char *choices = p.choices ? p.choices : "off|on";
        const char *bar = strchr(choices, '|');
        if (value) {
            snprintf(out, outMax, "%s", bar + 1);
        } else {
            snprintf(out, outMax, "%.*s", (int)(bar - choices), choices);
        }
    }

    const char *parseNumber(const Param &p, const char *value, int32_t *field)
    {
        long v;
        if (p.kind == Kind::Du && !strcmp(value, "off")) {
            v = 0;
        } else {
            char *end;
            v = strtol(value, &end, 10);
            if (end == value || *end) {
                return "not a number";
            }
        }
        if ((double)v < p.min || (double)v > p.max) {
            return "out of range";
        }
        *field = (int32_t)v;
        return nullptr;
    }

    const char *parseFlag(const Param &p, const char *value, bool *field)
    {
        char off[8];
        char on[8];
        flagName(p, false, off, sizeof(off));
        flagName(p, true, on, sizeof(on));
        if (!strcmp(value, on) || !strcmp(value, "1")) {
            *field = true;
        } else if (!strcmp(value, off) || !strcmp(value, "0")) {
            *field = false;
        } else {
            return p.choices ? p.choices : "off|on";
        }
        return nullptr;
    }

    const char *parseAngle(const Param &p, const char *value, float *field)
    {
        char *end;
        double degrees = strtod(value, &end);
        if (end == value || *end) {
            return "not a number";
        }
        double ratio = 2 * tan(degrees * RAD_PER_DEG);
        if (fabs(degrees) >= 90 || ratio < p.min || ratio > p.max) {
            return "out of range";
        }
        *field = (float)ratio;
        return nullptr;
    }

    const char *parseInto(const Param &p, const char *value, uint8_t *params)
    {
        void *field = params + p.offset;
        switch (p.kind) {
        case Kind::Du:
        case Kind::Count:
            return parseNumber(p, value, (int32_t *)field);
        case Kind::Axis:
            if (!strcmp(value, "z")) {
                *(Stepper::Axis *)field = Stepper::Axis::Z;
            } else if (!strcmp(value, "x")) {
                *(Stepper::Axis *)field = Stepper::Axis::X;
            } else {
                return "axis must be z or x";
            }
            return nullptr;
        case Kind::Flag:
            return parseFlag(p, value, (bool *)field);
        case Kind::Angle:
            return parseAngle(p, value, (float *)field);
        case Kind::Text:
            return "read-only";
        }
        return "bad param";
    }

    // Read-only text restored verbatim, "-" = empty; max is the field size.
    const char *restoreText(const Param &p, const char *value, uint8_t *params)
    {
        char *field = (char *)(params + p.offset);
        if (!strcmp(value, "-")) {
            field[0] = '\0';
            return nullptr;
        }
        if (strlen(value) >= (size_t)p.max) {
            return "too long";
        }
        strcpy(field, value);
        return nullptr;
    }

    int formatParam(const Param &p, const uint8_t *params, char *out, size_t outMax)
    {
        const void *field = params + p.offset;
        switch (p.kind) {
        case Kind::Du:
        case Kind::Count:
            return snprintf(out, outMax, "%s=%ld", p.name, (long)*(const int32_t *)field);
        case Kind::Axis:
            return snprintf(out, outMax, "%s=%s", p.name, *(const Stepper::Axis *)field == Stepper::Axis::Z ? "z" : "x");
        case Kind::Flag:
        {
            char name[8];
            flagName(p, *(const bool *)field, name, sizeof(name));
            return snprintf(out, outMax, "%s=%s", p.name, name);
        }
        case Kind::Angle:
            return snprintf(out, outMax, "%s=%.4g", p.name, atan((double)*(const float *)field / 2) / RAD_PER_DEG);
        case Kind::Text:
            return snprintf(out, outMax, "%s=%s", p.name, *(const char *)field ? (const char *)field : "-");
        }
        return 0;
    }

}

namespace Mode::Detail
{
    const Param *findParam(const Param *table, size_t count, const char *name)
    {
        for (size_t i = 0; i < count; i++) {
            if (!strcmp(table[i].name, name)) {
                return &table[i];
            }
        }
        return nullptr;
    }

    bool paramIsSet(const Param &p, const uint8_t *params)
    {
        return !p.required || *(const int32_t *)(params + p.offset) != 0;
    }

    const Param *firstUnsetParam(const Param *table, size_t count, const uint8_t *params)
    {
        for (size_t i = 0; i < count; i++) {
            if (!paramIsSet(table[i], params)) {
                return &table[i];
            }
        }
        return nullptr;
    }

    const char *parseParam(const Param &p, const char *value, uint8_t *params)
    {
        return parseInto(p, value, params);
    }

    const char *loadParamLine(const Param *table, size_t count, const char *line, uint8_t *params)
    {
        while (*line) {
            if (*line == ' ') {
                line++;
                continue;
            }
            size_t len = strcspn(line, " ");
            char token[TOKEN_MAX];
            if (len >= sizeof(token)) {
                return "param too long";
            }
            memcpy(token, line, len);
            token[len] = '\0';
            line += len;
            char *eq = strchr(token, '=');
            if (!eq) {
                return "expected name=value";
            }
            *eq = '\0';
            const Param *p = findParam(table, count, token);
            if (!p) {
                return "unknown param";
            }
            const char *refusal =
                p->kind == Kind::Text ? restoreText(*p, eq + 1, params) : parseInto(*p, eq + 1, params);
            if (refusal) {
                return refusal;
            }
        }
        return nullptr;
    }

    void describeParams(const Param *table, size_t count, const uint8_t *params, char *out, size_t outMax)
    {
        size_t n = 0;
        out[0] = '\0';
        for (size_t i = 0; i < count && n < outMax; i++) {
            if (i) {
                n += snprintf(out + n, outMax - n, " ");
            }
            if (n < outMax) {
                n += formatParam(table[i], params, out + n, outMax - n);
            }
        }
    }
}
