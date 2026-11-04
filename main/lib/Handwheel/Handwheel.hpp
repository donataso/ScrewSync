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
#ifndef Handwheel_h
#define Handwheel_h

#include <stdint.h>

namespace Handwheel
{
    // Starts polling the Z/X handwheel PCNT units (1 and 2).
    void begin();

    // Signed handwheel count since boot, 2x-decoded (Settings handwheelPpr * 2 per rev).
    int64_t z();
    int64_t x();
}

#endif
