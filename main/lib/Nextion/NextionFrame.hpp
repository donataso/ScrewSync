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
#ifndef NextionFrame_h
#define NextionFrame_h

#include <stddef.h>
#include <stdint.h>

namespace Nextion
{
    // Splits the display's return data into frames. Frames end with
    // 0xFF 0xFF 0xFF; fewer 0xFF are data.
    struct FrameParser
    {
        static constexpr size_t FRAME_MAX = 128;

        enum class Event : uint8_t
        {
            None,
            Frame,     // data/len hold a frame without its terminator
            Oversized, // a frame longer than FRAME_MAX ended
        };

        uint8_t data[FRAME_MAX];
        size_t len = 0;
        bool overflow = false;
        int ffRun = 0;

        // After Frame or Oversized, the next feed() starts a new frame.
        Event feed(uint8_t b);
        void reset();

    private:
        bool ended = false;
        void append(uint8_t b);
    };
}

#endif
