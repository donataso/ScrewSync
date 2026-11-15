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
#ifndef Machine_h
#define Machine_h

#include "Stepper.hpp"

// Single derived machine state. Nothing sets State directly -- each source
// announces only its own condition (spinning, mode engaged, flashing); state()
// derives Idle/Running/Flashing/EStop from those on every read. See
// docs/design.md "Machine state".
namespace Machine
{
    enum class State : uint8_t
    {
        Idle,
        Running,
        Flashing,
        EStop,
    };

    // Starts in EStop. Call once at startup, before any motion/OTA/web
    // subsystem begins.
    void begin();

    void setSpinning(bool spinning);
    void setModeEngaged(Stepper::Axis axis, bool engaged);

    // Requests entering/leaving Flashing. Entering while the machine is
    // Running trips EStop instead and returns false -- OTA must never start
    // while the machine is doing something.
    bool setFlashing(bool flashing);

    State state();

    // state() as the wire string: "idle"|"running"|"flashing"|"estop".
    const char *stateName();

    // True while no new web/WS/command traffic should be accepted (Flashing).
    bool blocksTraffic();

    // Idle, EStop, or Flashing for an update already in progress.
    bool allowsUpdate();

    // ISR-safe. Stops both axes at once, then trips the estop in thread context.
    void estopNow();

    bool estopOff();

    int64_t position(Stepper::Axis axis);
    int64_t positionOf(Stepper::Axis axis, int64_t steps);
    int64_t toSteps(Stepper::Axis axis, int64_t machineDu);
    void setOrigin(Stepper::Axis axis, int64_t du);
}

#endif
