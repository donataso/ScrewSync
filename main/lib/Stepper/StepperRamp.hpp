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
#ifndef StepperRamp_h
#define StepperRamp_h

#include <stdint.h>

namespace Stepper
{
    // Trapezoidal ramp: given the step rate just used and the distance
    // left to the target, returns the step rate for the next step --
    // accelerating toward maxSpeed, or decelerating toward speedStart in
    // time to land on the target exactly. Above maxSpeed (lowered
    // mid-move) it decelerates down to it rather than jumping. The floor is
    // speedStart, or maxSpeed when that is lower. No Zephyr/hardware dependency,
    // so it's host/native_sim testable on its own. speedRemainderMicro is
    // caller-kept state: the fraction of a step/s not yet applied.
    int32_t nextRampSpeed(int32_t currentSpeedStepsPerSec, int64_t distanceRemainingSteps,
                          int32_t accelerationStepsPerSec2, int32_t decelerationStepsPerSec2,
                          int32_t speedStartStepsPerSec, int32_t maxSpeedStepsPerSec,
                          int32_t &speedRemainderMicro);

    int64_t stoppingDistance(int32_t speedStepsPerSec, int32_t decelerationStepsPerSec2);

    // Distance to keep ahead of the axis so it can keep accelerating for
    // horizonUs and still stop by the target if no further extension comes.
    int64_t runAheadDistance(int32_t speedStepsPerSec, int32_t accelerationStepsPerSec2,
                             int32_t decelerationStepsPerSec2, int32_t maxSpeedStepsPerSec,
                             uint32_t horizonUs);
}

#endif
