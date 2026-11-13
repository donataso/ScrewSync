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
#include "ModeInternal.hpp"
#include "ModeMath.hpp"
#include "Settings.hpp"
#include <stddef.h>
#include <zephyr/sys/util.h>

namespace
{
    using namespace Mode::Detail;

    // Part: X pecks deeper each pass and returns to the engage point.
    struct Part
    {
        int32_t pitchDu; // feed per rev
        int32_t depthDu;
        int32_t peckDu; // 0 = full depth in one pass
        bool internal;
        bool spindleReverse;
    };

    constexpr double MAX_PITCH_DU = 1000000;
    constexpr double MAX_LENGTH_DU = 10000000;

    enum class Phase : uint8_t
    {
        Approach, // rapid to just short of the previous pass end
        Cut,
        Return,
    };

    Part g_params = {0, 0, 0, false, false};

    const Param PARAMS[] = {
        {"pitch", Kind::Du, offsetof(Part, pitchDu), 0, MAX_PITCH_DU, true, true},
        {"depth", Kind::Du, offsetof(Part, depthDu), 0, MAX_LENGTH_DU, false, true},
        {"peck", Kind::Du, offsetof(Part, peckDu), 0, MAX_LENGTH_DU, false},
        {"internal", Kind::Flag, offsetof(Part, internal), 0, 0, false},
        {"spindle", Kind::Flag, offsetof(Part, spindleReverse), 0, 0, false, false, "fwd|rev"},
    };

    int g_direction;
    int64_t g_start;
    int32_t g_pass;
    int32_t g_passes;
    Phase g_phase;
    bool g_entered;
    int64_t g_approach;
    SpindleFeed g_feed;

    int64_t passEnd(int32_t pass)
    {
        int64_t du = peckDepthDu(pass, g_passes, g_params.peckDu, g_params.depthDu);
        return g_start + g_direction * duToSteps(Stepper::Axis::X, (double)du);
    }

    void enter(Phase phase)
    {
        g_phase = phase;
        g_entered = false;
    }

    const char *startMode(int64_t)
    {
        const char *refusal = claim(Stepper::Axis::X);
        if (refusal) {
            return refusal;
        }
        g_direction = g_params.internal ? 1 : -1;
        g_start = Stepper::position(Stepper::Axis::X);
        g_passes = peckPasses(g_params.depthDu, g_params.peckDu);
        g_pass = 1;
        enter(Phase::Approach);
        return nullptr;
    }

    void tickMode(int64_t spindlePos)
    {
        switch (g_phase) {
        case Phase::Approach:
            if (!g_entered) {
                int64_t safe = duToSteps(Stepper::Axis::X, Settings::g_active.safeDistanceDu);
                int64_t previous = passEnd(g_pass - 1) - g_direction * safe;
                g_approach = (previous - g_start) * g_direction > 0 ? previous : g_start;
                g_entered = true;
            }
            if (moveAndWait(Stepper::Axis::X, g_approach)) {
                enter(Phase::Cut);
            }
            return;
        case Phase::Cut:
        {
            if (!g_entered) {
                g_feed.begin(Stepper::Axis::X, g_approach, passEnd(g_pass), spindlePos);
                g_entered = true;
            }
            if (g_feed.tick(spindlePos, overridden(g_params.pitchDu))) {
                enter(Phase::Return);
            }
            return;
        }
        case Phase::Return:
            if (moveAndWait(Stepper::Axis::X, g_start)) {
                if (++g_pass > g_passes) {
                    finish();
                } else {
                    enter(Phase::Approach);
                }
            }
            return;
        }
    }

    void progressMode(Mode::Progress &out)
    {
        static const char *const NAMES[] = {"approach", "cut", "return"};
        out.phase = NAMES[(size_t)g_phase];
        out.pass = g_pass;
        out.passes = g_passes;
    }
}

namespace Mode::Detail
{
    const Ops PART = {true, &g_params, sizeof(g_params), PARAMS, ARRAY_SIZE(PARAMS), startMode, tickMode,
                      nullptr, nullptr, progressMode, nullptr, &g_params.spindleReverse};
}
