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

namespace
{
    int32_t speedChange(int32_t rateStepsPerSec2, int32_t periodUs, int32_t &remainderMicro)
    {
        int64_t micro = (int64_t)rateStepsPerSec2 * periodUs + remainderMicro;
        remainderMicro = (int32_t)(micro % 1000000);
        return (int32_t)(micro / 1000000);
    }

    int64_t isqrt(int64_t value)
    {
        int64_t root = 0;
        for (int64_t bit = (int64_t)1 << 62; bit != 0; bit >>= 2) {
            if (value >= root + bit) {
                value -= root + bit;
                root = (root >> 1) + bit;
            } else {
                root >>= 1;
            }
        }
        return root;
    }
}

namespace Stepper
{
    int32_t nextRampSpeed(int32_t currentSpeedStepsPerSec, int64_t distanceRemainingSteps,
                          int32_t accelerationStepsPerSec2, int32_t decelerationStepsPerSec2,
                          int32_t speedStartStepsPerSec, int32_t maxSpeedStepsPerSec,
                          int32_t &speedRemainderMicro)
    {
        int32_t lastPeriodUs = 1000000 / currentSpeedStepsPerSec;

        int64_t stopDistance = stoppingDistance(currentSpeedStepsPerSec, decelerationStepsPerSec2);

        int32_t speed;
        if (distanceRemainingSteps <= stopDistance) {
            // Closed form, so per-step rounding cannot build up and leave speed at the target.
            speed = (int32_t)isqrt(2 * (int64_t)decelerationStepsPerSec2 * distanceRemainingSteps);
        } else if (currentSpeedStepsPerSec > maxSpeedStepsPerSec) {
            speed = currentSpeedStepsPerSec - speedChange(decelerationStepsPerSec2, lastPeriodUs, speedRemainderMicro);
            if (speed < maxSpeedStepsPerSec) {
                speed = maxSpeedStepsPerSec;
            }
        } else {
            speed = currentSpeedStepsPerSec + speedChange(accelerationStepsPerSec2, lastPeriodUs, speedRemainderMicro);
            if (speed > maxSpeedStepsPerSec) {
                speed = maxSpeedStepsPerSec;
            }
        }
        int32_t floor = speedStartStepsPerSec < maxSpeedStepsPerSec ? speedStartStepsPerSec : maxSpeedStepsPerSec;
        if (speed < floor) {
            speed = floor;
        }
        return speed;
    }

    int32_t brakeSpeed(int32_t currentSpeedStepsPerSec, int32_t decelerationStepsPerSec2,
                       int32_t floorSpeedStepsPerSec, int32_t &speedRemainderMicro)
    {
        int32_t lastPeriodUs = 1000000 / currentSpeedStepsPerSec;
        int32_t speed =
            currentSpeedStepsPerSec - speedChange(decelerationStepsPerSec2, lastPeriodUs, speedRemainderMicro);
        return speed < floorSpeedStepsPerSec ? floorSpeedStepsPerSec : speed;
    }

    int64_t stoppingDistance(int32_t speedStepsPerSec, int32_t decelerationStepsPerSec2)
    {
        return ((int64_t)speedStepsPerSec * speedStepsPerSec) / (2 * (int64_t)decelerationStepsPerSec2);
    }

    int64_t runAheadDistance(int32_t speedStepsPerSec, int32_t accelerationStepsPerSec2,
                             int32_t decelerationStepsPerSec2, int32_t maxSpeedStepsPerSec,
                             uint32_t horizonUs)
    {
        int64_t reachable = speedStepsPerSec + (int64_t)accelerationStepsPerSec2 * horizonUs / 1000000;
        int32_t speed = reachable > maxSpeedStepsPerSec ? maxSpeedStepsPerSec : (int32_t)reachable;
        return (int64_t)speed * horizonUs / 1000000 + stoppingDistance(speed, decelerationStepsPerSec2) + 1;
    }
}
