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
#ifndef ProbeInput_h
#define ProbeInput_h

// The probe's digital input (app.overlay's probe-gpios): interrupt-driven,
// NO/NC polarity from Settings::active.probePolarityNc. onEdge runs
// synchronously from the GPIO ISR, so it must be ISR-safe (see
// Stepper::haltNow).
namespace ProbeInput
{
    using Callback = void (*)(bool contact);

    void begin(Callback onEdge);

    // Logical contact state (polarity-corrected); safe to poll any time.
    bool contact();

    constexpr bool fromRaw(bool raw, bool polarityNc) { return polarityNc ? raw : !raw; }
}

#endif
