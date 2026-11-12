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
#include "ModeInternal.hpp"
#include "Encoder.hpp"
#include "Motion.hpp"
#include "Settings.hpp"
#include "SyncRatio.hpp"
#include <math.h>
#include <stdlib.h>
#include <zephyr/sys/util.h>

namespace
{
    const Settings::Axis &cfgOf(Stepper::Axis axis)
    {
        return axis == Stepper::Axis::Z ? Settings::g_active.z : Settings::g_active.x;
    }

    void endOnClamp(Stepper::Axis axis)
    {
        Mode::Detail::finish(Stepper::isDisabled(axis) ? "axis disabled" : "travel limit");
    }
}

namespace Mode::Detail
{
    int64_t duToSteps(Stepper::Axis axis, double du)
    {
        return Motion::duToSteps(axis, du);
    }

    double stepsToDu(Stepper::Axis axis, int64_t steps)
    {
        return Motion::stepsToDu(axis, steps);
    }

    int32_t rapidSpeed(Stepper::Axis axis)
    {
        return cfgOf(axis).speedManualMove;
    }

    int32_t duPerMinToStepsPerSec(Stepper::Axis axis, int32_t duPerMin)
    {
        const Settings::Axis &cfg = cfgOf(axis);
        int64_t speed = llround(fabs((double)duPerMin) * cfg.motorSteps / cfg.screwPitchDu / 60.0);
        if (speed < 1) {
            return 1;
        }
        return speed > cfg.speedManualMove ? cfg.speedManualMove : (int32_t)speed;
    }

    int32_t countsPerRev()
    {
        return Motion::spindleCountsPerRev();
    }

    bool moveAndWait(Stepper::Axis axis, int64_t target, int32_t speed)
    {
        if (Stepper::moveTo(axis, target, speed ? speed : rapidSpeed(axis)) != target) {
            endOnClamp(axis);
            return false;
        }
        return Stepper::arrived(axis) && Stepper::position(axis) == target;
    }

    int64_t helixPhase(const Sync::Link &link, int64_t refAxisPos)
    {
        return Sync::helixPhase(Encoder::origin(), refAxisPos, link.stepsPerCount());
    }

    int64_t helixOrigin(const Sync::Link &link, int64_t refAxisPos, int64_t aheadCounts, int64_t spindlePos)
    {
        int64_t phase = helixPhase(link, refAxisPos) + spindleSign() * aheadCounts;
        return Sync::nextOnPhase(spindlePos, phase, countsPerRev(), spindleSign());
    }

    void SpindleFeed::begin(Stepper::Axis feedAxis, int64_t fromSteps, int64_t toSteps, int64_t spindlePos)
    {
        axis = feedAxis;
        pos = (double)fromSteps;
        from = fromSteps;
        to = toSteps;
        lastSpindle = spindlePos;
    }

    bool SpindleFeed::tick(int64_t spindlePos, int32_t pitchDu)
    {
        const Settings::Axis &cfg = cfgOf(axis);
        double stepsPerCount = (double)cfg.motorSteps * abs(pitchDu) / ((double)cfg.screwPitchDu * countsPerRev());
        int direction = to >= from ? 1 : -1;
        pos += (double)spindleTravel(lastSpindle, spindlePos) * stepsPerCount * direction;
        lastSpindle = spindlePos;
        double lo = (double)(direction > 0 ? from : to);
        double hi = (double)(direction > 0 ? to : from);
        pos = CLAMP(pos, lo, hi);
        int64_t target = (int64_t)llround(pos);
        if (Stepper::moveTo(axis, target, rapidSpeed(axis)) != target) {
            endOnClamp(axis);
            return false;
        }
        return target == to && Stepper::arrived(axis) && Stepper::position(axis) == to;
    }
}
