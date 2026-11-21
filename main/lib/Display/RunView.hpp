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
#ifndef RunView_h
#define RunView_h

#include "Mode.hpp"
#include <stdint.h>

namespace Display::Detail
{
    // runView on displays/nextion-5in/pages/page0.nx.
    enum class RunState : uint8_t
    {
        Closed = 0,
        Armed = 1, // Prepare pressed, nothing moves yet
        Running = 2,
    };

    // Replaces the mode panel from Prepare until the job ends.
    struct RunView
    {
        bool armed = false;
        Mode::Id mode = Mode::Id::Gear;

        static bool available(Mode::Id id)
        {
            return id != Mode::Id::Gcode && id != Mode::Id::Probe;
        }

        void arm(Mode::Id current)
        {
            armed = available(current);
            mode = current;
        }

        void cancel()
        {
            armed = false;
        }

        RunState update(Mode::Id current, bool running)
        {
            if (running || current != mode) {
                armed = false;
            }
            if (!available(current)) {
                return RunState::Closed;
            }
            if (running) {
                return RunState::Running;
            }
            return armed ? RunState::Armed : RunState::Closed;
        }
    };
}

#endif
