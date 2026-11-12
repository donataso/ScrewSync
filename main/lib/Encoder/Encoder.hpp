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
#ifndef Encoder_h
#define Encoder_h

#include <stdint.h>

namespace Encoder
{
    // Forward rotation increases the count.
    enum class Direction : uint8_t
    {
        Stopped,
        Forward,
        Reverse,
    };

    void begin();

    // Signed spindle encoder count since boot, 2x-decoded (Settings::active.encoderPpr * 2 per rev).
    int64_t position();

    // position(), with Settings::active.encoderBacklash counts of reversal slop
    // absorbed by a deadband filter. Use this for thread/feed sync; use
    // position() where the raw count (e.g. RPM) is wanted instead.
    int64_t filteredPosition();

    // Count of spindle angle 0; threads are phased to it.
    int64_t origin();
    void setOrigin(int64_t count);

    // Direction of the latest filteredPosition() move. Only tracked while it
    // is polled often; Stopped after a pause in either.
    Direction direction();
}

#endif
