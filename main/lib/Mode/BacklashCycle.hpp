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
#ifndef BacklashCycle_h
#define BacklashCycle_h

#include <stddef.h>
#include <stdint.h>

// Backlash release measurement (docs/calibration.md). Moves are carriage
// positions; samples are motor positions, so the backlash setting does not
// affect the result.
namespace BacklashCycle
{
    constexpr int MAX_RUNS = 9;

    enum class Phase : uint8_t
    {
        Idle,
        Approach,
        Release,
        BackOff,
        Done,
    };

    enum class Watch : uint8_t
    {
        None,
        Contact,
        Release,
    };

    struct Params
    {
        int dir; // +1 or -1: approach direction, toward the workpiece
        int64_t probeDstSteps;
        int64_t backOffSteps;
        int64_t marginSteps;
        int runs;
    };

    struct State
    {
        Phase phase = Phase::Idle;
        int64_t phaseStart = 0;
        int64_t contactMotor = 0;
        int run = 0;
        int64_t samples[MAX_RUNS] = {};
    };

    struct Input
    {
        int64_t pos;
        int64_t motorPos;
        bool contactNow;
        bool latched; // the edge interrupt halted the axis at latchedPos/latchedMotor
        int64_t latchedPos;
        int64_t latchedMotor;
    };

    struct Result
    {
        bool move = false;
        bool reprobeFeed = false;
        int64_t target = 0;
        bool done = false;
        int64_t backlashSteps = 0; // median, valid when done
        const char *error = nullptr;
    };

    // nullptr, or the refusal.
    const char *begin(State &s, const Params &p, int64_t startPos, bool contactAtStart);
    Result tick(State &s, const Params &p, const Input &in);
    Watch watching(const State &s);

    // Sorts samples in place.
    int64_t median(int64_t *samples, int count);
}

#endif
