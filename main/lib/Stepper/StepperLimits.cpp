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
#include "StepperLimits.hpp"
#include <math.h>

namespace Stepper
{
    int64_t maxTravelSteps(int32_t maxTravelMm, int32_t motorSteps, int32_t screwPitchDu)
    {
        double travelDu = (double)maxTravelMm * 10000.0;
        return (int64_t)llround(travelDu * motorSteps / screwPitchDu);
    }

    int64_t backlashSteps(int32_t backlashDu, int32_t motorSteps, int32_t screwPitchDu)
    {
        return (int64_t)backlashDu * motorSteps / screwPitchDu;
    }

    int64_t clampToTravel(int64_t targetSteps, int64_t maxTravelSteps)
    {
        if (targetSteps > maxTravelSteps) {
            return maxTravelSteps;
        }
        if (targetSteps < -maxTravelSteps) {
            return -maxTravelSteps;
        }
        return targetSteps;
    }

    int64_t clampToLag(int64_t targetSteps, int64_t positionSteps, int64_t maxLagSteps)
    {
        if (targetSteps > positionSteps + maxLagSteps) {
            return positionSteps + maxLagSteps;
        }
        if (targetSteps < positionSteps - maxLagSteps) {
            return positionSteps - maxLagSteps;
        }
        return targetSteps;
    }

    void trackStep(bool positive, int64_t backlash, int64_t &axisPos, int64_t &motorPos)
    {
        if (positive) {
            if (motorPos >= axisPos) {
                axisPos++;
            }
            motorPos++;
        } else {
            if (motorPos <= axisPos - backlash) {
                axisPos--;
            }
            motorPos--;
        }
    }
}
