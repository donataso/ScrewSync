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
        return axis == Stepper::Axis::Z ? Settings::active.z : Settings::active.x;
    }

    const char *moveToSteps(Stepper::Axis axis, int64_t target)
    {
        if (Mode::owns(axis)) {
            return "mode running, stop first";
        }
        Stepper::moveTo(axis, target);
        return nullptr;
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
        return Settings::active.encoderPpr * 2;
    }

    bool idle()
    {
        return Machine::state() == Machine::State::Idle && !Mode::running();
    }

    const char *moveTo(Stepper::Axis axis, int64_t du, int64_t &targetSteps)
    {
        targetSteps = duToSteps(axis, (double)du);
        return moveToSteps(axis, targetSteps);
    }

    const char *moveBy(Stepper::Axis axis, int64_t du, int64_t &targetSteps)
    {
        targetSteps = Stepper::position(axis) + duToSteps(axis, (double)du);
        return moveToSteps(axis, targetSteps);
    }

    const char *zero(Stepper::Axis axis, int64_t du)
    {
        if (Mode::owns(axis)) {
            return "mode running, stop first";
        }
        Stepper::zero(axis, duToSteps(axis, (double)du));
        return nullptr;
    }

    void stop(Stepper::Axis axis)
    {
        Stepper::moveTo(axis, Stepper::position(axis));
    }

    void setAxisEnabled(Stepper::Axis axis, bool enabled)
    {
        if (!enabled) {
            Mode::onAxisDisabled(axis);
        }
        RunSettings::setAxisDisabled(axis, !enabled);
    }

    int64_t zeroSpindle()
    {
        int64_t pos = Encoder::position();
        Encoder::setOrigin(pos % spindleCountsPerRev());
        return pos;
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
