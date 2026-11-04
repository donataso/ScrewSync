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
#ifndef SyncRatio_h
#define SyncRatio_h

#include <stdint.h>

namespace Sync
{
    // Axis steps per spindle count.
    double ratio(int32_t motorSteps, int32_t duprDu, int32_t starts, int32_t screwPitchDu,
                 int32_t encoderStepsPerRev);

    int64_t ratioTarget(int64_t originAxisPosSteps, int64_t originSpindlePos, int64_t spindlePos,
                         int32_t motorSteps, int32_t duprDu, int32_t starts, int32_t screwPitchDu,
                         int32_t encoderStepsPerRev);

    // A spindle count where an axis at refAxisPos lies on the helix through
    // axis 0 at originCount.
    int64_t helixPhase(int64_t originCount, int64_t refAxisPos, double stepsPerCount);

    // The first count on phase at or ahead of spindlePos; direction is +-1.
    int64_t nextOnPhase(int64_t spindlePos, int64_t phase, int64_t countsPerRev, int direction);

    // The first count on phase after from, up to and including to.
    bool crossedPhase(int64_t from, int64_t to, int64_t phase, int64_t countsPerRev, int64_t &at);
}

#endif
