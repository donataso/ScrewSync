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
#include "StepperRamp.hpp"

namespace Stepper
{
    int32_t nextRampSpeed(int32_t currentSpeedStepsPerSec, int64_t distanceRemainingSteps,
                           int32_t accelerationStepsPerSec2, int32_t speedStartStepsPerSec,
                           int32_t maxSpeedStepsPerSec)
    {
        int32_t lastPeriodUs = 1000000 / currentSpeedStepsPerSec;
        int32_t dv = (int32_t)((int64_t)accelerationStepsPerSec2 * lastPeriodUs / 1000000);
        if (dv < 1) {
            dv = 1;
        }

        int64_t stopDistance = stoppingDistance(currentSpeedStepsPerSec, accelerationStepsPerSec2);

        int32_t speed;
        if (distanceRemainingSteps <= stopDistance) {
            speed = currentSpeedStepsPerSec - dv;
        } else if (currentSpeedStepsPerSec > maxSpeedStepsPerSec) {
            speed = currentSpeedStepsPerSec - dv < maxSpeedStepsPerSec ? maxSpeedStepsPerSec
                                                                       : currentSpeedStepsPerSec - dv;
        } else {
            speed = currentSpeedStepsPerSec + dv > maxSpeedStepsPerSec ? maxSpeedStepsPerSec
                                                                       : currentSpeedStepsPerSec + dv;
        }
        int32_t floor = speedStartStepsPerSec < maxSpeedStepsPerSec ? speedStartStepsPerSec : maxSpeedStepsPerSec;
        if (speed < floor) {
            speed = floor;
        }
        return speed;
    }

    int64_t stoppingDistance(int32_t speedStepsPerSec, int32_t accelerationStepsPerSec2)
    {
        return ((int64_t)speedStepsPerSec * speedStepsPerSec) / (2 * accelerationStepsPerSec2);
    }

    int64_t runAheadDistance(int32_t speedStepsPerSec, int32_t accelerationStepsPerSec2,
                             int32_t maxSpeedStepsPerSec, uint32_t horizonUs)
    {
        int64_t reachable = speedStepsPerSec + (int64_t)accelerationStepsPerSec2 * horizonUs / 1000000;
        int32_t speed = reachable > maxSpeedStepsPerSec ? maxSpeedStepsPerSec : (int32_t)reachable;
        return (int64_t)speed * horizonUs / 1000000 + stoppingDistance(speed, accelerationStepsPerSec2) + 1;
    }
}
