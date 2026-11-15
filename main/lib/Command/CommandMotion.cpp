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
#include "CommandInternal.hpp"
#include "Jog.hpp"
#include "Motion.hpp"
#include "RunSettings.hpp"
#include <zephyr/sys/util.h>

namespace
{
    using Command::Detail::skipSpaces;

    void jogStep(const char *v, char *out, size_t outMax)
    {
        long du;
        if (!strcmp(v, "next")) {
            RunSettings::nextJogStep();
            snprintf(out, outMax, "ok step=%ld", (long)RunSettings::jogStepDu());
        } else if (!*v) {
            snprintf(out, outMax, "ok step=%ld", (long)RunSettings::jogStepDu());
        } else if (!Command::Detail::parseLong(v, du) || !RunSettings::setJogStepDu((int32_t)du)) {
            snprintf(out, outMax, "err jog step <deci-microns> out of range");
        } else {
            snprintf(out, outMax, "ok step=%ld", du);
        }
    }

    void jogPress(Stepper::Axis axis, const char *axisTok, const char *rest, char *out, size_t outMax)
    {
        int direction = rest[0] == '+' ? 1 : -1;
        const char *modifier = skipSpaces(rest + 1);
        bool isHold = !strcmp(modifier, "hold");
        if (!strcmp(modifier, "release")) {
            Jog::release(axis);
            snprintf(out, outMax, "ok %s %c release", axisTok, rest[0]);
            return;
        }
        if (*modifier && !isHold) {
            snprintf(out, outMax, "err jog %s +|- [hold|release]", axisTok);
            return;
        }
        const char *refusal = isHold ? Jog::hold(axis, direction) : Jog::tap(axis, direction);
        if (refusal) {
            snprintf(out, outMax, "err jog %s: %s", axisTok, refusal);
        } else {
            snprintf(out, outMax, "ok %s %c%s", axisTok, rest[0], isHold ? " hold" : "");
        }
    }
}

namespace Command::Detail
{
    void jog(const char *args, char *out, size_t outMax)
    {
        const char *a = skipSpaces(args);
        if (!strncmp(a, "step", 4) && (a[4] == '\0' || a[4] == ' ')) {
            jogStep(skipSpaces(a + 4), out, outMax);
            return;
        }
        char axisTok[4];
        Stepper::Axis axis;
        const char *rest;
        if (!takeAxis(a, "jog",
                      "jog step [<deci-microns>|next] | z|x +|- [hold|release] | resolution <multiplier> | "
                      "speed <du/min|0> | status",
                      axisTok, axis, rest, out, outMax)) {
            return;
        }

        char *end;
        if (!*rest || !strcmp(rest, "status")) {
            snprintf(out, outMax, "ok axis=%s resolution=%g", axisTok, (double)Jog::resolution(axis));
        } else if (rest[0] == '+' || rest[0] == '-') {
            jogPress(axis, axisTok, rest, out, outMax);
        } else if (!strncmp(rest, "resolution ", 11)) {
            float v = strtof(rest + 11, &end);
            if (end == rest + 11 || *end || v <= 0) {
                snprintf(out, outMax, "err jog %s resolution <multiplier> (>0)", axisTok);
            } else {
                Jog::setResolution(axis, v);
                snprintf(out, outMax, "ok %s resolution=%g", axisTok, (double)v);
            }
        } else if (!strncmp(rest, "speed ", 6)) {
            long v = strtol(rest + 6, &end, 10);
            if (end == rest + 6 || *end || v < 0) {
                snprintf(out, outMax, "err jog %s speed <du/min|0>", axisTok);
            } else {
                int64_t steps = v == 0 ? 0 : MAX(Motion::duToSteps(axis, (double)v / 60.0), 1);
                Stepper::setSpeedOverride(axis, (int32_t)MIN(steps, INT32_MAX));
                snprintf(out, outMax, "ok %s speed=%ld", axisTok, v);
            }
        } else {
            snprintf(out, outMax, "err jog '%s'", rest);
        }
    }
}
