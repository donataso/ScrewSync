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
#ifndef SwitchDebounce_h
#define SwitchDebounce_h

#include <stdint.h>

// A level change counts after `samples` equal reads in a row.
struct SwitchDebounce
{
    bool stable;
    uint8_t count;

    // True on the read that confirms a press.
    bool pressed(bool level, uint8_t samples)
    {
        if (level == stable) {
            count = 0;
            return false;
        }
        if (++count < samples) {
            return false;
        }
        stable = level;
        count = 0;
        return level;
    }
};

#endif
