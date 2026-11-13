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
#ifndef JogRatio_h
#define JogRatio_h

#include <stdint.h>

namespace Jog
{
    // Whole motor steps for a handwheel count change: motorSteps per
    // handwheelPpr counts, scaled by resolutionMultiplier. Rounded to
    // the nearest step; the difference stays in remainder for the next
    // call (start with 0), so slow turning at a fine resolution adds up.
    int64_t jogSteps(int64_t handwheelCountDelta, int32_t motorSteps, float handwheelPpr,
                     float resolutionMultiplier, double &remainder);
}

#endif
