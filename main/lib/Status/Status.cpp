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
#include "Status.hpp"
#include "Machine.hpp"
#include "Motion.hpp"
#include "ProbeInput.hpp"
#include "Rpm.hpp"
#include "RunSettings.hpp"
#include "Stepper.hpp"

namespace
{
    Status::Axis axisStatus(Stepper::Axis axis)
    {
        return {Stepper::position(axis), Stepper::isMoving(axis), !Stepper::isDisabled(axis)};
    }
}

namespace Status
{
    Snapshot get()
    {
        Snapshot s;
        s.z = axisStatus(Stepper::Axis::Z);
        s.x = axisStatus(Stepper::Axis::X);
        s.mode = Mode::name(Mode::current());
        s.running = Mode::running();
        s.ready = Mode::ready();
        s.units = RunSettings::unitsName();
        s.jogStepDu = RunSettings::jogStepDu();
        Mode::Gear g = Mode::gear();
        s.gear = {g.reverse ? -g.pitchDu : g.pitchDu, g.axis == Stepper::Axis::Z ? "z" : "x", g.stopAfterDu};
        s.job = Mode::progress();
        s.feedOverridePct = Mode::feedOverride();
        s.error = Mode::lastError();
        s.params[0] = '\0';
        Mode::describe(Mode::current(), s.params, sizeof(s.params));
        s.probeContact = ProbeInput::contact();
        s.rpm = Rpm::value();
        s.state = Machine::stateName();
        s.angleDeg = Motion::spindleAngleDeg();
        return s;
    }
}
