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
#ifndef Jog_h
#define Jog_h

#include "Stepper.hpp"
#include <stdint.h>

namespace Jog
{
    void begin();

    // Jog buttons: tap moves one RunSettings::jogStepDu() step; hold,
    // repeated every JOG_REPEAT_INTERVAL_MS while pressed, keeps the axis
    // accelerating.
    // Return nullptr on success, else the reason it was refused.
    const char *tap(Stepper::Axis axis, int direction);
    const char *hold(Stepper::Axis axis, int direction);

    // Hold button let go: decelerates now instead of running out the hold.
    void release(Stepper::Axis axis);
}

#endif
