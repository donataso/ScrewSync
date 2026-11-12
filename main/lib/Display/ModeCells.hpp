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
#ifndef ModeCells_h
#define ModeCells_h

#include "Mode.hpp"
#include <stddef.h>

// Display layout of each mode's panel, row-major: slots 0-9 a 2-column
// grid of 5 rows, slots 10-15 two 3-column rows over grid rows 4 and 5.
// A mode uses one or the other per row.
namespace ModeCells
{
    constexpr size_t MAX = 16;
    constexpr size_t NARROW_FIRST = 10;

    // A value cell at 0 shows "not set" if its param is required, else "off".
    enum class Kind : uint8_t
    {
        Length,
        Pitch,
        Feed,  // per rev
        Speed, // per minute
        Count,
        Angle,  // degrees
        Toggle, // two buttons, one value each
        Picker, // opens the thread picker page; no param; slot 15 only
        Latch,  // on/off flag, one button lit while on; narrow slots only
        Gap,
    };

    struct Cell
    {
        Kind kind;
        const char *param;
        const char *label;  // value cells
        const char *labelA; // Toggle; nullptr = arrows along the mode's axis ("-", "+")
        const char *valueA;
        const char *labelB;
        const char *valueB;
        const char *when; // "name=value": shown only while that param has that value
    };

    const char *label(Mode::Id id);
    const Cell *cells(Mode::Id id, size_t &count);
}

#endif
