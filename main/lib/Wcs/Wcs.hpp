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
#include <stddef.h>
#include <stdint.h>

// Work coordinate systems: work = machine - offset, all in du. RAM only.
namespace Wcs
{
    constexpr size_t COUNT = 6; // G54..G59

    size_t active();
    int64_t offset(size_t index, Stepper::Axis axis);
    void setOffset(size_t index, Stepper::Axis axis, int64_t du);

    // Work position in the active system.
    int64_t position(Stepper::Axis axis);
    int64_t toMachine(Stepper::Axis axis, int64_t workDu);
    // The present position reads as workDu in the active system.
    void setPosition(Stepper::Axis axis, int64_t workDu);
}

#endif
