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
#ifndef Motion_h
#define Motion_h

#include "Stepper.hpp"
#include <stdint.h>

namespace Motion
{
    int64_t duToSteps(Stepper::Axis axis, double du);
    double stepsToDu(Stepper::Axis axis, int64_t steps);
    int32_t spindleCountsPerRev();

    // No mode running and the machine is Idle.
    bool idle();

    // Each returns nullptr on success, else the reason.
    const char *moveTo(Stepper::Axis axis, int64_t du, int64_t &targetSteps);
    const char *moveBy(Stepper::Axis axis, int64_t du, int64_t &targetSteps);
    const char *zero(Stepper::Axis axis, int64_t du);

    void stop(Stepper::Axis axis);
    void setAxisEnabled(Stepper::Axis axis, bool enabled);

    // The current spindle angle becomes 0 deg; returns the count it was taken at.
    int64_t zeroSpindle();
    float spindleAngleDeg();
}

#endif
