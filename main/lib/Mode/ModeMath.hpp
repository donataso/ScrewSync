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
#ifndef ModeMath_h
#define ModeMath_h

#include <stdint.h>

namespace Mode::Detail
{
    // A per-minute feed as a step rate, at least 1 and at most maxSpeed.
    int32_t feedStepsPerSec(int32_t duPerMin, int32_t motorSteps, int32_t screwPitchDu, int32_t maxSpeed);
    // Step rate of the main axis so a move of vectorDu path length runs at
    // feedDuPerMin; cap when the feed is 0.
    int32_t vectorFeedStepsPerSec(double vectorDu, int32_t feedDuPerMin, int64_t mainSteps, int32_t cap);
    // Axis steps per spindle count of a feed per rev.
    double spindleFeedStepsPerCount(int32_t motorSteps, int32_t pitchDu, int32_t screwPitchDu,
                                    int32_t countsPerRev);

    // Pecking: passes of peckDu down to depthDu, the last one shorter. peckDu 0 = one pass.
    int32_t peckPasses(int32_t depthDu, int32_t peckDu);
    int64_t peckDepthDu(int32_t pass, int32_t passes, int32_t peckDu, int32_t depthDu);

    // Quarter ellipse of half-axes a (along Z) and b (along X), from angle 0
    // to a quarter turn. Point as du offsets from the corner, x <= 0 = into
    // the stock.
    void ellipsePoint(double a, double b, bool concave, double angle, double &zDu, double &xDu);
    // Profile length per radian at angle, du; at least 1.
    double ellipseSpeed(double a, double b, bool concave, double angle);
}

#endif
