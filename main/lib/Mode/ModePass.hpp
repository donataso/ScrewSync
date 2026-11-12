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
#ifndef ModePass_h
#define ModePass_h

#include "ModeInternal.hpp"
#include "Sync.hpp"

// Multi-pass engine shared by turn, thread and slot. Geometry is relative
// to the engage point: main axis cuts from there over a length, aux axis
// steps deeper each pass. Each pass: plunge, cut, retract, return to start
// (approaching it in the cut direction so backlash is taken up before the
// cut). After the last pass the aux axis returns to the engage point.
namespace Mode::Detail
{
    enum class PassFeed : uint8_t
    {
        Spindle, // feed per rev
        Phased,  // feed per rev, every pass on the same spindle phase (threads)
        Timed,   // feed per minute, no spindle
    };

    struct PassJob
    {
        Stepper::Axis main;
        Stepper::Axis aux;
        int auxSign; // aux direction into the work
        double lengthDu;
        double depthDu;
        int32_t levels;
        int32_t starts;
        bool squaredDepth;   // deeper first passes
        bool retractToStart; // else retract from the pass depth
        double endReductionDu; // shortens each successive pass
        PassFeed feed;
        const int32_t *pitchDu;       // read every tick
        const int32_t *speedDuPerMin; // Timed
        bool reverse;                 // feeds +main
        float taper;                  // main Z only, aux X follows
        double flankTan;              // alternating flank infeed: Z shift per depth, 0 = radial
        int32_t finishLevels;         // spring passes at full depth

        int cutDirection;
        int64_t mainStart;
        int64_t auxStart;
        uint8_t phase;
        bool entered;
        int32_t level;
        int32_t startIndex;
        int32_t skipLevels; // applied when the next depth level starts
        int32_t offsetLevel; // flankOffset() memo
        int64_t offsetSteps;
        int64_t retractAux;
        Sync::Link link;
        SpindleFeed spindleFeed;
    };

    // Claims both axes; config fields must be set.
    const char *passBegin(PassJob &job, int64_t spindlePos);
    void passTick(PassJob &job, int64_t spindlePos);
    void passSkip(PassJob &job);
    void passProgress(const PassJob &job, Progress &out);
}

#endif
