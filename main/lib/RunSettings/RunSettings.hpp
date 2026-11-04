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
#ifndef RunSettings_h
#define RunSettings_h

#include "Stepper.hpp"
#include "Mode.hpp"
#include <stddef.h>
#include <stdint.h>

// Runtime/operational state -- its own NVS namespace, separate from
// Settings' machine config -- for things that change during normal use
// rather than one-time calibration. Every setter persists immediately.
namespace RunSettings
{
    enum class Units : uint8_t
    {
        Mm,
        Inch,
    };

    // Restores everything at boot, including the manual enable/disable
    // switch into Stepper. Never re-engages a mode -- that always starts off.
    void load();

    // The manual enable/disable switch: persists across reboots on its own,
    // independent of any mode.
    void setAxisDisabled(Stepper::Axis axis, bool disabled);

    Units units();
    const char *unitsName();
    bool parseUnits(const char *name, Units &out);
    void setUnits(Units units);

    int32_t jogStepDu();
    bool jogStepValid(int32_t du);
    // False if out of range.
    bool setJogStepDu(int32_t du);

    Mode::Id mode();
    void setMode(Mode::Id mode);

    // Per-mode parameter struct, stored under the mode's name. False (out
    // untouched) when absent or stored with a different size.
    bool loadModeParams(const char *key, void *out, size_t size);
    void saveModeParams(const char *key, const void *params, size_t size);
}

#endif
