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
#ifndef DisplayText_h
#define DisplayText_h

#include "ModeCells.hpp"

#include <stddef.h>

// Text the display shows, built from values only.
namespace Display::Detail
{
    const char *unitSuffix(bool inch);
    // mm to 3 decimals, inch to 4, never "-0.000".
    void formatLength(double du, bool inch, char *out, size_t outMax);
    void trimZeros(char *s);
    // Value of name in a "name=value ..." list.
    bool paramValue(const char *params, const char *name, char *out, size_t outMax);

    bool cellShown(const ModeCells::Cell &cell, const char *params);
    // Value with its unit; false when it is 0.
    bool formatCellValue(const ModeCells::Cell &cell, const char *raw, bool inch, char *out, size_t outMax);

    void groupLabel(const char *name, char *out, size_t outMax);
    // "M10x1.25" -> "M10×1.25".
    void designationLabel(const char *designation, char *out, size_t outMax);
    // Next item of a comma-separated list; false at its end.
    bool nextItem(const char *&list, char *out, size_t outMax);
    bool hasItem(const char *list, const char *name);
}

#endif
