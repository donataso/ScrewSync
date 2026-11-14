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
#ifndef JogButton_h
#define JogButton_h

#include <stdint.h>

// A jog button, the same as on the display: a short press is a tap, a long
// press repeats hold until the release.
struct JogButton
{
    enum class Event : uint8_t
    {
        None,
        Tap,
        Hold,
        Release,
    };

    bool active;
    bool holding;
    // Set by a button held at boot or a refused command; no events until the release.
    bool blocked;
    uint32_t pressMs;
    uint32_t lastHoldMs;

    Event update(bool down, uint32_t nowMs, uint32_t holdDelayMs, uint32_t repeatMs)
    {
        if (!down) {
            bool wasActive = active && !blocked;
            bool wasHolding = holding;
            active = holding = blocked = false;
            return wasActive ? (wasHolding ? Event::Release : Event::Tap) : Event::None;
        }
        if (blocked) {
            return Event::None;
        }
        if (!active) {
            active = true;
            pressMs = nowMs;
            return Event::None;
        }
        if (!holding ? nowMs - pressMs >= holdDelayMs : nowMs - lastHoldMs >= repeatMs) {
            holding = true;
            lastHoldMs = nowMs;
            return Event::Hold;
        }
        return Event::None;
    }
};

#endif
