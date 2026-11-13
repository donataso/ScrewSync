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
#ifndef StepperLimits_h
#define StepperLimits_h

#include <stdint.h>

// No Zephyr/hardware dependency, so it's host/native_sim testable alone.
namespace Stepper
{
    // Settings::Axis.maxTravelMm converted to motor steps -- the hard
    // sanity bound around the zero origin, enforced in moveTo().
    int64_t maxTravelSteps(int32_t maxTravelMm, int32_t motorSteps, int32_t screwPitchDu);

    int64_t clampToTravel(int64_t targetSteps, int64_t maxTravelSteps);

    // Settings::Axis.backlashDu converted to motor steps -- how far the
    // motor must travel to take up lead-screw slack before the reported
    // position starts moving, on a direction reversal.
    int64_t backlashSteps(int32_t backlashDu, int32_t motorSteps, int32_t screwPitchDu);

    // One motor step: the motor position always moves, the axis position
    // only once the backlash of a reversal is taken up. backlash is used
    // on a negative step only.
    void trackStep(bool positive, int64_t backlash, int64_t &axisPos, int64_t &motorPos);
}

#endif
