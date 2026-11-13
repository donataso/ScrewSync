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
#include "NextionFrame.hpp"

namespace Nextion
{
    void FrameParser::append(uint8_t b)
    {
        if (len < sizeof(data)) {
            data[len++] = b;
        } else {
            overflow = true;
        }
    }

    void FrameParser::reset()
    {
        len = 0;
        overflow = false;
        ffRun = 0;
        ended = false;
    }

    FrameParser::Event FrameParser::feed(uint8_t b)
    {
        if (ended) {
            reset();
        }
        if (b == 0xFF) {
            if (++ffRun == 3) {
                ended = true;
                return overflow ? Event::Oversized : Event::Frame;
            }
            return Event::None;
        }
        for (; ffRun > 0; ffRun--) {
            append(0xFF);
        }
        append(b);
        return Event::None;
    }
}
