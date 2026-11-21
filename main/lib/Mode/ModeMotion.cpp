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
#include "ModeMath.hpp"
#include "Encoder.hpp"
#include "Motion.hpp"
#include "Rpm.hpp"
#include "Settings.hpp"
#include "SyncRatio.hpp"
#include <math.h>
#include <stdlib.h>
#include <initializer_list>
#include <zephyr/sys/util.h>

namespace
{
    const Settings::Axis &cfgOf(Stepper::Axis axis)
    {
        return axis == Stepper::Axis::Z ? Settings::g_active.z : Settings::g_active.x;
    }

    int64_t g_helixRef[2];

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
        return cfgOf(axis).speedRapidSteps;
    }

    int32_t duPerMinToStepsPerSec(Stepper::Axis axis, int32_t duPerMin)
    {
        const Settings::Axis &cfg = cfgOf(axis);
        return feedStepsPerSec(duPerMin, cfg.motorSteps, cfg.screwPitchDu, cfg.speedRapidSteps);
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
        return Sync::helixPhase(Encoder::origin(), refAxisPos - g_helixRef[(size_t)link.axis], link.stepsPerCount());
    }

    double rampAccel(Stepper::Axis axis)
    {
        return (double)cfgOf(axis).accelerationSteps * RAMP_ACCEL_PCT / 100;
    }

    double spindleCountsPerSec()
    {
        return fabs((double)Rpm::value()) * countsPerRev() / 60;
    }

    void HelixEntry::begin(int64_t phaseCount, int fixedDirection)
    {
        phase = phaseCount;
        direction = fixedDirection;
        rotation = fixedDirection;
        halfCounts = 0;
        planned = false;
        started = false;
    }

    int64_t HelixEntry::target(Sync::Link &link, int64_t spindlePos)
    {
        if (!started) {
            if (planned && (double)(rotation * (spindlePos - link.originSpindlePos)) > -halfCounts) {
                started = true;
            } else {
                int32_t rpm = Rpm::value();
                int measured = rpm > 0 ? 1 : (rpm < 0 ? -1 : 0);
                if (direction == 0 && measured != 0 && measured != rotation) {
                    rotation = measured;
                    planned = false;
                }
                if (rotation == 0) {
                    return link.originAxisPos;
                }
                double half = Sync::rampHalfCounts(link.stepsPerCount(), spindleCountsPerSec(), rampAccel(link.axis));
                double travel = (double)(rotation * (spindlePos - link.originSpindlePos));
                if (!planned || travel > -half) {
                    int64_t from = spindlePos + rotation * (int64_t)ceil(half);
                    link.originSpindlePos = Sync::nextOnPhase(from, phase, countsPerRev(), rotation);
                    planned = true;
                }
                halfCounts = half;
            }
        }
        double travel = (double)(rotation * (spindlePos - link.originSpindlePos));
        double offset = Sync::rampOffset(travel, halfCounts, rotation * link.stepsPerCount());
        return link.originAxisPos + llround(offset);
    }

    bool HelixEntry::joined(const Sync::Link &link, int64_t spindlePos) const
    {
        return started && (double)(rotation * (spindlePos - link.originSpindlePos)) >= halfCounts;
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
        double stepsPerCount = spindleFeedStepsPerCount(cfg.motorSteps, pitchDu, cfg.screwPitchDu, countsPerRev());
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

namespace Mode
{
    void restartHelix()
    {
        Detail::Guard guard;
        for (Stepper::Axis axis : {Stepper::Axis::Z, Stepper::Axis::X}) {
            g_helixRef[(size_t)axis] = Stepper::position(axis);
        }
    }

    void clearHelix(Stepper::Axis axis)
    {
        Detail::Guard guard;
        g_helixRef[(size_t)axis] = 0;
    }
}
