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
#ifndef HomeInput_h
#define HomeInput_h

#include "Stepper.hpp"

// The home inputs of the pin map: interrupt-driven, polarity from the invert
// option of the assignment. onEdge runs synchronously from the GPIO ISR, so
// it must be ISR-safe (see Stepper::haltNow). It may be nullptr.
namespace HomeInput
{
    using Callback = void (*)(Stepper::Axis axis, bool active);

    void begin(Callback onEdge);

    bool present(Stepper::Axis axis);

    bool active(Stepper::Axis axis);

    bool sensors();
}

#endif
