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
#ifndef JogStep_h
#define JogStep_h

#include <stddef.h>
#include <stdint.h>

namespace Jog
{
    constexpr int32_t MAX_STEP_DU = 1000000;
    constexpr size_t STEP_PRESET_COUNT = 3;

    const int32_t *stepPresets(bool inch);
    bool isStepPreset(int32_t du, bool inch);

    // The presets, then custom (0 = none), then the first preset again.
    int32_t nextStep(int32_t current, int32_t custom, bool inch);

    // Adds detents × multiplier display increments (0.001 mm or 0.0001 in), clamped.
    int32_t dialStep(int32_t du, int32_t detents, int32_t multiplier, bool inch);
    // Increments per detent: more at a faster turn.
    int32_t dialMultiplier(uint32_t msPerDetent);
}

#endif
