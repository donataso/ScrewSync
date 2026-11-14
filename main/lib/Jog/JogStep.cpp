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
#include "JogStep.hpp"
#include <math.h>

namespace
{
    const int32_t PRESETS_MM_DU[Jog::STEP_PRESET_COUNT] = {100, 1000, 10000};
    const int32_t PRESETS_INCH_DU[Jog::STEP_PRESET_COUNT] = {254, 2540, 25400};
    constexpr double INCREMENT_MM_DU = 10.0;
    constexpr double INCREMENT_INCH_DU = 2.54;

    struct DialSpeed
    {
        uint32_t maxMsPerDetent;
        int32_t multiplier;
    };

    constexpr DialSpeed DIAL_SPEEDS[] = {{30, 100}, {80, 10}};
}

namespace Jog
{
    const int32_t *stepPresets(bool inch)
    {
        return inch ? PRESETS_INCH_DU : PRESETS_MM_DU;
    }

    bool isStepPreset(int32_t du, bool inch)
    {
        const int32_t *presets = stepPresets(inch);
        for (size_t i = 0; i < STEP_PRESET_COUNT; i++) {
            if (presets[i] == du) {
                return true;
            }
        }
        return false;
    }

    int32_t nextStep(int32_t current, int32_t custom, bool inch)
    {
        const int32_t *presets = stepPresets(inch);
        for (size_t i = 0; i + 1 < STEP_PRESET_COUNT; i++) {
            if (presets[i] == current) {
                return presets[i + 1];
            }
        }
        bool onLastPreset = current == presets[STEP_PRESET_COUNT - 1];
        if (onLastPreset && custom > 0 && !isStepPreset(custom, inch)) {
            return custom;
        }
        return presets[0];
    }

    int32_t dialStep(int32_t du, int32_t detents, int32_t multiplier, bool inch)
    {
        double increment = inch ? INCREMENT_INCH_DU : INCREMENT_MM_DU;
        double maxUnits = floor(MAX_STEP_DU / increment);
        double units = round(du / increment) + (double)detents * multiplier;
        units = units < 1 ? 1 : (units > maxUnits ? maxUnits : units);
        return (int32_t)llround(units * increment);
    }

    int32_t dialMultiplier(uint32_t msPerDetent)
    {
        for (const DialSpeed &s : DIAL_SPEEDS) {
            if (msPerDetent <= s.maxMsPerDetent) {
                return s.multiplier;
            }
        }
        return 1;
    }
}
