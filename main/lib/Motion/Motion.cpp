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
#include "Motion.hpp"
#include "Encoder.hpp"
#include "Machine.hpp"
#include "Mode.hpp"
#include "RunSettings.hpp"
#include "Settings.hpp"
#include <math.h>

namespace
{
    const Settings::Axis &cfgOf(Stepper::Axis axis)
    {
        return axis == Stepper::Axis::Z ? Settings::g_active.z : Settings::g_active.x;
    }
}

namespace Motion
{
    int64_t duToSteps(Stepper::Axis axis, double du)
    {
        const Settings::Axis &cfg = cfgOf(axis);
        return (int64_t)llround(du * cfg.motorSteps / cfg.screwPitchDu);
    }

    double stepsToDu(Stepper::Axis axis, int64_t steps)
    {
        const Settings::Axis &cfg = cfgOf(axis);
        return (double)steps * cfg.screwPitchDu / cfg.motorSteps;
    }

    int32_t spindleCountsPerRev()
    {
        return Settings::g_active.encoderPpr * 2;
    }

    bool idle()
    {
        return Machine::state() == Machine::State::Idle && !Mode::busy();
    }

    void setAxisEnabled(Stepper::Axis axis, bool enabled)
    {
        if (!enabled) {
            Mode::onAxisDisabled(axis);
        }
        RunSettings::setAxisDisabled(axis, !enabled);
    }

    void setSpindleAngle(double deg)
    {
        int32_t countsPerRev = spindleCountsPerRev();
        int64_t origin = (Encoder::position() - (int64_t)llround(deg * countsPerRev / 360.0)) % countsPerRev;
        Encoder::setOrigin(origin < 0 ? origin + countsPerRev : origin);
    }

    float spindleAngleDeg()
    {
        int32_t countsPerRev = spindleCountsPerRev();
        int64_t count = (Encoder::position() - Encoder::origin()) % countsPerRev;
        if (count < 0) {
            count += countsPerRev;
        }
        return (float)count * 360.0f / (float)countsPerRev;
    }
}
