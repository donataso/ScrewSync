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
#include "GcodeFlavors.hpp"
#include "GcodeLathecode.hpp"
#include <string.h>

namespace
{
    using namespace Gcode;

    Lathecode g_lathecode;
    Flavor *const g_flavors[] = {&g_lathecode};
    constexpr size_t FLAVOR_COUNT = sizeof(g_flavors) / sizeof(g_flavors[0]);
}

namespace Gcode
{
    Flavor *find(const char *id)
    {
        for (size_t i = 0; i < FLAVOR_COUNT; i++) {
            if (strcmp(g_flavors[i]->id(), id) == 0) {
                return g_flavors[i];
            }
        }
        return nullptr;
    }

    size_t count()
    {
        return FLAVOR_COUNT;
    }

    Flavor *at(size_t i)
    {
        return g_flavors[i];
    }
}
