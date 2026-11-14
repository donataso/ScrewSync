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
#include "JogRatio.hpp"
#include <math.h>

namespace Jog
{
    int64_t jogSteps(int64_t countDelta, int32_t motorSteps, float handwheelPpr, float resolutionMultiplier,
                     double &remainder)
    {
        double stepsPerPulse = (double)motorSteps / (double)handwheelPpr * (double)resolutionMultiplier;
        double exact = (double)countDelta * stepsPerPulse + remainder;
        double whole = round(exact);
        remainder = exact - whole;
        return (int64_t)whole;
    }
}
