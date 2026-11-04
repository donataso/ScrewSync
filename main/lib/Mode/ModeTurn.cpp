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
#include "ModePass.hpp"
#include <stddef.h>
#include <zephyr/sys/util.h>

namespace
{
    using namespace Mode::detail;

    // Main axis Z = turning (depth on X), X = facing (depth on Z).
    struct Turn
    {
        int32_t pitchDu;
        bool reverse;
        Stepper::Axis axis;
        int32_t lengthDu;
        int32_t depthDu;
        int32_t passes;
        bool internal;
        bool spindleReverse;
    };

    constexpr double MAX_PITCH_DU = 1000000;
    constexpr double MAX_LENGTH_DU = 10000000;
    constexpr double MAX_PASSES = 1000;

    Turn g_params = {0, false, Stepper::Axis::Z, 0, 0, 5, false, false};

    const Param PARAMS[] = {
        {"pitch", Kind::Du, offsetof(Turn, pitchDu), 0, MAX_PITCH_DU, true, true},
        {"dir", Kind::Flag, offsetof(Turn, reverse), 0, 0, false, false, "-|+"},
        {"axis", Kind::Axis, offsetof(Turn, axis), 0, 0, false},
        {"length", Kind::Du, offsetof(Turn, lengthDu), 0, MAX_LENGTH_DU, false, true},
        {"depth", Kind::Du, offsetof(Turn, depthDu), 0, MAX_LENGTH_DU, false, true},
        {"passes", Kind::Count, offsetof(Turn, passes), 1, MAX_PASSES, false},
        {"internal", Kind::Flag, offsetof(Turn, internal), 0, 0, false},
        {"spindle", Kind::Flag, offsetof(Turn, spindleReverse), 0, 0, false, false, "fwd|rev"},
    };

    PassJob g_job;

    const char *startMode(int64_t spindlePos)
    {
        bool facing = g_params.axis == Stepper::Axis::X;
        g_job = {};
        g_job.main = g_params.axis;
        g_job.aux = facing ? Stepper::Axis::Z : Stepper::Axis::X;
        g_job.auxSign = g_params.internal ? 1 : -1;
        g_job.lengthDu = g_params.lengthDu;
        g_job.depthDu = g_params.depthDu;
        g_job.levels = g_params.passes;
        g_job.starts = 1;
        g_job.feed = PassFeed::Spindle;
        g_job.pitchDu = &g_params.pitchDu;
        g_job.reverse = g_params.reverse;
        return passBegin(g_job, spindlePos);
    }

    void tickMode(int64_t spindlePos) { passTick(g_job, spindlePos); }
    void progressMode(Mode::Progress &out) { passProgress(g_job, out); }
    void skipMode() { passSkip(g_job); }
}

namespace Mode::detail
{
    const Ops TURN = {true, &g_params, sizeof(g_params), PARAMS, ARRAY_SIZE(PARAMS), startMode, tickMode,
                      nullptr, nullptr, progressMode, skipMode, &g_params.spindleReverse};
}
