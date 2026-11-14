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
#ifndef CommandInternal_h
#define CommandInternal_h

#include "Stepper.hpp"
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

namespace Command::Detail
{
    using Handler = void (*)(const char *args, char *out, size_t outMax);

    inline const char *skipSpaces(const char *s)
    {
        while (*s == ' ') {
            s++;
        }
        return s;
    }

    inline bool parseLong(const char *s, long &out)
    {
        char *end;
        out = strtol(s, &end, 10);
        return end != s && !*end;
    }

    inline void reply(char *out, size_t outMax, const char *refusal, const char *okFmt, const char *okArg)
    {
        if (refusal) {
            snprintf(out, outMax, "err %s", refusal);
        } else {
            snprintf(out, outMax, okFmt, okArg);
        }
    }

    bool parseAxis(const char *token, Stepper::Axis &axis);

    // Leading "z"|"x" token: axisTok and the rest after it. Replies the
    // error and returns false otherwise.
    bool takeAxis(const char *args, const char *verb, const char *usage, char (&axisTok)[4], Stepper::Axis &axis,
                  const char *&rest, char *out, size_t outMax);

    bool refuseUnlessIdle(char *out, size_t outMax);

    void wifi(const char *args, char *out, size_t outMax);

    void settings(const char *args, char *out, size_t outMax);
    void info(const char *args, char *out, size_t outMax);
    void display(const char *args, char *out, size_t outMax);
    void pins(const char *args, char *out, size_t outMax);
    void units(const char *args, char *out, size_t outMax);
    void ws(const char *args, char *out, size_t outMax);

    void jog(const char *args, char *out, size_t outMax);
    void move(const char *args, char *out, size_t outMax);
    void zero(const char *args, char *out, size_t outMax);
    void enable(const char *args, char *out, size_t outMax);

    void mode(const char *args, char *out, size_t outMax);
    void start(const char *args, char *out, size_t outMax);
    void stop(const char *args, char *out, size_t outMax);
    void skip(const char *args, char *out, size_t outMax);
    void override(const char *args, char *out, size_t outMax);
    void gear(const char *args, char *out, size_t outMax);
    void async(const char *args, char *out, size_t outMax);
    void turn(const char *args, char *out, size_t outMax);
    void part(const char *args, char *out, size_t outMax);
    void slot(const char *args, char *out, size_t outMax);
    void ellipse(const char *args, char *out, size_t outMax);
    void thread(const char *args, char *out, size_t outMax);
    void threads(const char *args, char *out, size_t outMax);
    void probe(const char *args, char *out, size_t outMax);
}

#endif
