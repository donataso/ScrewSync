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
#ifndef RpmCalc_h
#define RpmCalc_h

#include <stdint.h>

namespace Rpm
{
    // Spindle RPM from an encoder count delta over an elapsed window.
    // encoderCountsPerRev is the x2-decoded count (Settings::active.encoderPpr
    // * 2). Signed: negative means the spindle turned in reverse over the
    // window. No Zephyr/hardware dependency, so it's host/native_sim
    // testable alone.
    int32_t rpmFromDelta(int64_t deltaCounts, uint32_t elapsedMs, int32_t encoderCountsPerRev);
}

#endif
