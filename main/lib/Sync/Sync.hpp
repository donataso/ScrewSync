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
#ifndef Sync_h
#define Sync_h

#include "Stepper.hpp"
#include <stdint.h>

namespace Sync
{
    // Spindle-to-axis gear link. Not thread-safe: owned by one mode,
    // driven from the Mode runner under its lock.
    //
    // engage() snapshots the current spindle and axis position as the
    // origin, so the axis picks up the ratio from wherever it is instead of
    // jumping to an absolute target.
    struct Link
    {
        Stepper::Axis axis;
        int32_t duprDu; // per spindle rev, deci-microns, signed
        int32_t starts;
        int64_t originSpindlePos;
        int64_t originAxisPos;

        void engage(int64_t spindlePos);
        int64_t target(int64_t spindlePos) const;
        // Signed, as target() applies it.
        double stepsPerCount() const;

        // Re-engages from the current position.
        void setDupr(int32_t duprDu, int64_t spindlePos);
    };
}

#endif
