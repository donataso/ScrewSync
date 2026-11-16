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
#ifndef InputSubscribers_h
#define InputSubscribers_h

#include <stdint.h>

struct InputSubscribers
{
    enum class Change : uint8_t
    {
        None,
        Started,
        Stopped,
    };

    uint32_t mask = 0;

    bool has(int slot) const
    {
        return mask >> slot & 1;
    }

    Change set(int slot, bool on)
    {
        uint32_t before = mask;
        mask = on ? (mask | (uint32_t)1 << slot) : (mask & ~((uint32_t)1 << slot));
        if (!before && mask) {
            return Change::Started;
        }
        return before && !mask ? Change::Stopped : Change::None;
    }
};

#endif
