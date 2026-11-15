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
#ifndef Wcs_h
#define Wcs_h

#include "Stepper.hpp"
#include "WcsRules.hpp"
#include <stddef.h>
#include <stdint.h>

// Work coordinate systems: work = machine - offset, all in du. G54 is active
// at boot. With home switches the offsets are saved; without, only G54
// exists and its offset is lost at reboot.
namespace Wcs
{
    void begin();

    size_t active();
    int64_t offset(size_t index, Stepper::Axis axis);

    const char *select(size_t index);

    // Work position in the active system.
    int64_t position(Stepper::Axis axis);
    int64_t toMachine(Stepper::Axis axis, int64_t workDu);
    const char *setPosition(size_t index, Stepper::Axis axis, int64_t workDu);
}

#endif
