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
#ifndef Homing_h
#define Homing_h

#include "Settings.hpp"
#include "Stepper.hpp"

namespace Homing
{
    const char *start();
    bool active();

    bool homed(Stepper::Axis axis);
    void clearHomed(Stepper::Axis axis);
    void settingsChanged(const Settings::Conf &before, const Settings::Conf &after);

    bool blocked();
    void refreshGate();

    // Mode lock held.
    void tick();
    void stop();

    // The HomeInput callback; ISR context.
    void onEdge(Stepper::Axis axis, bool active);
}

#endif
