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
#ifndef EncoderFilter_h
#define EncoderFilter_h

#include "Encoder.hpp"

#include <stdint.h>

namespace Encoder
{
    // Deadband filter: absorbs backlash counts of reversal slop, and tracks
    // the direction of the filtered count while it is polled often.
    struct Filter
    {
        static constexpr int64_t STALE_MS = 100;

        int64_t filtered = 0;
        bool seeded = false;
        bool lastInvert = false;
        int32_t lastBacklash = 0;
        int64_t lastPollMs = 0;
        int64_t lastMoveMs = 0;
        Direction lastDirection = Direction::Stopped;

        // A changed invert or backlash restarts the filter at raw.
        int64_t update(int64_t raw, int64_t nowMs, bool invert, int32_t backlash);
        Direction direction(int64_t nowMs) const;
    };
}

#endif
