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
#include "Settings.hpp"
#include <stddef.h>
#include <zephyr/sys/util.h>

namespace
{
    using namespace Mode::Detail;

    // Z feeds, X steps deeper each pass; no spindle.
    struct Slot
    {
        int32_t speedDuPerMin;
        bool reverse;
        int32_t lengthDu;
        int32_t depthDu;
        int32_t passes;
        bool internal;
    };

    constexpr double MAX_SPEED_DU_PER_MIN = 60000000;
    constexpr double MAX_LENGTH_DU = 10000000;
    constexpr double MAX_PASSES = 1000;

    Slot g_params = {0, false, 0, 0, 5, false};

    const Param PARAMS[] = {
        {"speed", Kind::Du, offsetof(Slot, speedDuPerMin), 0, MAX_SPEED_DU_PER_MIN, true, true},
        {"dir", Kind::Flag, offsetof(Slot, reverse), 0, 0, false, false, "-|+"},
        {"length", Kind::Du, offsetof(Slot, lengthDu), 0, MAX_LENGTH_DU, false, true},
        {"depth", Kind::Du, offsetof(Slot, depthDu), 0, MAX_LENGTH_DU, false, true},
        {"passes", Kind::Count, offsetof(Slot, passes), 1, MAX_PASSES, false},
        {"internal", Kind::Flag, offsetof(Slot, internal), 0, 0, false},
    };

    PassJob g_job;

    const char *startMode(int64_t spindlePos)
    {
        g_job = {};
        g_job.main = Stepper::Axis::Z;
        g_job.aux = Stepper::Axis::X;
        g_job.auxSign = g_params.internal ? 1 : -1;
        g_job.lengthDu = g_params.lengthDu;
        g_job.depthDu = g_params.depthDu;
        g_job.levels = g_params.passes;
        g_job.starts = 1;
        g_job.retractToStart = true;
        g_job.endReductionDu = Settings::g_active.slotLeftReductionDu;
        g_job.feed = PassFeed::Timed;
        g_job.pitchDu = &g_params.speedDuPerMin;
        g_job.speedDuPerMin = &g_params.speedDuPerMin;
        g_job.reverse = g_params.reverse;
        return passBegin(g_job, spindlePos);
    }

    void tickMode(int64_t spindlePos)
    {
        passTick(g_job, spindlePos);
    }
    void progressMode(Mode::Progress &out)
    {
        passProgress(g_job, out);
    }
    void skipMode()
    {
        passSkip(g_job);
    }
}

namespace Mode::Detail
{
    const Ops SLOT = {true, &g_params, sizeof(g_params), PARAMS, ARRAY_SIZE(PARAMS), startMode, tickMode,
                      nullptr, nullptr, progressMode, skipMode};
}
