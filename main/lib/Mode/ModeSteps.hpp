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
#ifndef ModeSteps_h
#define ModeSteps_h

#include "GcodeCore.hpp"
#include "GcodeNgc.hpp"
#include "Stepper.hpp"
#include "Sync.hpp"

// Runs one G-code Step with Z and X driven together: the axis with the
// larger du delta is "main" and moves at the computed speed (rapid for G0,
// feed-derived for G1, phased to the thread helix for G33); "aux" chases
// main's fraction of progress. Targets are work coordinates. G38.x moves
// one axis until the probe input changes; the interrupt halts it with no
// deceleration, so the caller must take the position again afterwards.
namespace Mode::Detail
{
    bool isMotion(Gcode::StepKind kind);

    // SetPosition, Motors, SpindleAngle.
    void applyMachineStep(const Gcode::Step &s);

    // The probe interrupt halts no axis after this.
    void probeDisarm();

    // NGC parameters, shared by MDI and jobs; kept until reboot.
    Gcode::Params &params();
    // Settings as #<_name>, and #<_probe_contact>.
    Gcode::ParamSource &paramSource();

    struct StepMotion
    {
        Stepper::Axis mainAxis;
        Stepper::Axis auxAxis;
        int64_t mainFrom, mainTo, auxFrom, auxTo;
        double vectorDu; // Feed only: path length for the per-minute speed split
        Sync::Link thread;
        bool probeStarted;
        // G38 done: whether it tripped, and where (work du, main axis).
        bool probeFound;
        int32_t probeTripDu;
        int32_t probeMotorDu; // without backlash compensation

        void begin(const Gcode::Step &s, int64_t spindlePos);
        // True once done. error is set when a travel clamp or a disabled axis
        // stopped it. spindleGated: G0/G1 hold while the spindle is stopped.
        bool drive(const Gcode::Step &s, int64_t spindlePos, bool spindleGated, const char *&error);
    };
}

#endif
