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
#ifndef ProbeCycle_h
#define ProbeCycle_h

#include <stdint.h>

// Pure probe touch-off cycle state machine (docs/probe.md "Cycle"),
// hardware-independent so it's unit-testable: step counts and a contact
// flag in, a move/error/done decision out. ModeProbe drives the real
// Stepper/ProbeInput from this.
namespace ProbeCycle
{
    enum class Phase : uint8_t
    {
        Idle,
        Approach,
        BackOff1,
        Reprobe,
        BackOff2,
        Done,
    };

    struct Params
    {
        int dir;               // +1 or -1: approach direction, toward the workpiece
        int64_t probeDstSteps;  // max travel for the first touch
        int64_t backOffSteps;   // retract after each touch
        int64_t marginSteps;    // reprobe travel past backOff
        int64_t offsetSteps;    // trigger point to surface
    };

    struct State
    {
        Phase phase = Phase::Idle;
        int64_t phaseStart = 0;
        int64_t contactSteps = 0; // the reprobe's latched contact position
    };

    struct Result
    {
        bool move = false; // command a move toward target at the feed below
        bool reprobeFeed = false;
        int64_t target = 0;
        bool done = false;
        int64_t surfaceSteps = 0; // valid when done
        const char *error = nullptr;
    };

    // nullptr, or the refusal ("contact present").
    const char *begin(State &s, const Params &p, int64_t startPos, bool contactAtStart);

    // One tick. justContacted/contactPos: the probe interrupt already
    // halted the axis at contactPos this tick (only meaningful during
    // Approach/Reprobe, the phases watching() arms). A latch with contact
    // already gone is an error.
    Result tick(State &s, const Params &p, int64_t currentPos, bool contactNow, bool justContacted,
                int64_t contactPos);

    // True while the cycle wants the contact interrupt armed.
    bool watching(const State &s);
}

#endif
