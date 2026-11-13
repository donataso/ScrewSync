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
#include "EncoderFilter.hpp"

namespace Encoder
{
    int64_t Filter::update(int64_t raw, int64_t nowMs, bool invert, int32_t backlash)
    {
        bool polled = nowMs - lastPollMs <= STALE_MS;
        lastPollMs = nowMs;
        int64_t previous = filtered;
        if (!seeded || invert != lastInvert || backlash != lastBacklash) {
            filtered = raw;
            seeded = true;
            lastInvert = invert;
            lastBacklash = backlash;
            previous = raw;
        } else if (raw > filtered + backlash) {
            filtered = raw - backlash;
        } else if (raw < filtered - backlash) {
            filtered = raw + backlash;
        }
        if (filtered != previous) {
            if (!polled) {
                lastDirection = Direction::Stopped;
            } else if (filtered > previous) {
                lastDirection = Direction::Forward;
            } else {
                lastDirection = Direction::Reverse;
            }
            lastMoveMs = nowMs;
        }
        return filtered;
    }

    Direction Filter::direction(int64_t nowMs) const
    {
        return nowMs - lastMoveMs <= STALE_MS ? lastDirection : Direction::Stopped;
    }
}
