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
#include "SyncRatio.hpp"
#include <math.h>

namespace
{
    int64_t mod(int64_t v, int64_t m)
    {
        return ((v % m) + m) % m;
    }
}

namespace Sync
{
    double ratio(int32_t motorSteps, int32_t duprDu, int32_t starts, int32_t screwPitchDu,
                 int32_t encoderStepsPerRev)
    {
        return (double)motorSteps * duprDu * starts / ((double)screwPitchDu * encoderStepsPerRev);
    }

    int64_t ratioTarget(int64_t originAxisPosSteps, int64_t originSpindlePos, int64_t spindlePos,
                        int32_t motorSteps, int32_t duprDu, int32_t starts, int32_t screwPitchDu,
                        int32_t encoderStepsPerRev)
    {
        double r = ratio(motorSteps, duprDu, starts, screwPitchDu, encoderStepsPerRev);
        return originAxisPosSteps + (int64_t)llround((double)(spindlePos - originSpindlePos) * r);
    }

    int64_t helixPhase(int64_t originCount, int64_t refAxisPos, double stepsPerCount)
    {
        return originCount + (stepsPerCount != 0 ? llround((double)refAxisPos / stepsPerCount) : 0);
    }

    int64_t nextOnPhase(int64_t spindlePos, int64_t phase, int64_t countsPerRev, int direction)
    {
        int64_t ahead = mod(direction * (phase - spindlePos), countsPerRev);
        return spindlePos + direction * ahead;
    }

    bool crossedPhase(int64_t from, int64_t to, int64_t phase, int64_t countsPerRev, int64_t &at)
    {
        if (to > from) {
            at = from + 1 + mod(phase - from - 1, countsPerRev);
            return at <= to;
        }
        if (to < from) {
            at = from - 1 - mod(from - 1 - phase, countsPerRev);
            return at >= to;
        }
        return false;
    }
}
