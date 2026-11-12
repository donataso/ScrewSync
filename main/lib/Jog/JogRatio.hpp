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
    // One handwheel turn = one leadscrew turn, scaled by resolutionMultiplier.
    // Relative to the engagement origin; unused while Sync drives the axis.
    int64_t jogTarget(int64_t originAxisPosSteps, int64_t originHandwheelCount, int64_t handwheelCount,
                      int32_t motorSteps, float handwheelPpr, float resolutionMultiplier);
}

#endif
