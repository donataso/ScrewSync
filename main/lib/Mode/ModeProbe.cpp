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
#include "ProbeCycle.hpp"
#include "ProbeInput.hpp"
#include "Settings.hpp"
#include <stddef.h>
#include <zephyr/sys/util.h>

namespace
{
    using namespace Mode::detail;

    struct Probe
    {
        int32_t probeDstDu;
        int32_t offsetDu;
    };

    constexpr double MAX_LENGTH_DU = 10000000;
    constexpr int32_t REPROBE_MARGIN_DU = 5000;

    Probe g_params = {0, 0};

    const Param PARAMS[] = {
        {"probe-dst", Kind::Du, offsetof(Probe, probeDstDu), 0, MAX_LENGTH_DU, false, true},
        {"offset", Kind::Du, offsetof(Probe, offsetDu), 0, MAX_LENGTH_DU, false, false},
    };

    // Set by Mode::probeGo(); a plain start is refused.
    Stepper::Axis g_axis = Stepper::Axis::Z;
    bool g_reverse = false;
    bool g_goPending = false;

    ProbeCycle::State g_cycle;
    int64_t g_commanded; // re-issuing an unchanged target would undo a contact halt

    // Shared with Mode::probeEdge() (ISR context): plain flags, no mode
    // lock -- the lock is a k_mutex, not ISR-safe.
    volatile bool g_watching = false;
    volatile bool g_contactLatched = false;
    volatile int64_t g_latchedPos = 0;

    int probeDirection() { return g_reverse ? 1 : -1; }

    ProbeCycle::Params cycleParams()
    {
        return {
            probeDirection(),
            duToSteps(g_axis, g_params.probeDstDu),
            duToSteps(g_axis, Settings::active.probeBackOffDu),
            duToSteps(g_axis, REPROBE_MARGIN_DU),
            duToSteps(g_axis, g_params.offsetDu),
        };
    }

    const char *startMode(int64_t)
    {
        if (!g_goPending) {
            return "use probe go";
        }
        g_goPending = false;
        const char *refusal = claim(g_axis);
        if (refusal) {
            return refusal;
        }
        const char *err = ProbeCycle::begin(g_cycle, cycleParams(), Stepper::position(g_axis), ProbeInput::contact());
        if (err) {
            return err;
        }
        g_contactLatched = false;
        g_commanded = INT64_MIN;
        g_watching = ProbeCycle::watching(g_cycle);
        return nullptr;
    }

    void tickMode(int64_t)
    {
        int64_t currentPos = Stepper::position(g_axis);
        bool contactNow = ProbeInput::contact();
        bool justContacted = g_contactLatched;
        int64_t contactPos = g_latchedPos;
        if (justContacted) {
            g_contactLatched = false;
            g_commanded = INT64_MIN;
        }

        ProbeCycle::Result r = ProbeCycle::tick(g_cycle, cycleParams(), currentPos, contactNow, justContacted, contactPos);
        g_watching = ProbeCycle::watching(g_cycle);

        if (r.error) {
            g_watching = false;
            finish(r.error);
            return;
        }
        if (r.done) {
            Stepper::zero(g_axis, currentPos - r.surfaceSteps);
            g_watching = false;
            finish();
            return;
        }
        if (r.move && r.target != g_commanded) {
            g_commanded = r.target;
            int32_t feedDuPerMin = r.reprobeFeed ? Settings::active.reprobeFeedDuPerMin : Settings::active.probeFeedDuPerMin;
            int32_t speed = duPerMinToStepsPerSec(g_axis, feedDuPerMin);
            if (Stepper::moveTo(g_axis, r.target, speed) != r.target) {
                g_watching = false;
                finish(Stepper::isDisabled(g_axis) ? "axis disabled" : "travel limit");
            }
        }
    }

    void progressMode(Mode::Progress &out)
    {
        switch (g_cycle.phase)
        {
        case ProbeCycle::Phase::Approach:
            out.phase = "approach";
            break;
        case ProbeCycle::Phase::BackOff1:
        case ProbeCycle::Phase::BackOff2:
            out.phase = "back off";
            break;
        case ProbeCycle::Phase::Reprobe:
            out.phase = "reprobe";
            break;
        default:
            out.phase = "run";
            break;
        }
    }
}

namespace Mode::detail
{
    const Ops PROBE = {true,       &g_params, sizeof(g_params), PARAMS, ARRAY_SIZE(PARAMS), startMode, tickMode,
                       nullptr,    nullptr,   progressMode,     nullptr, nullptr,            nullptr};
}

namespace Mode
{
    const char *probeGo(Stepper::Axis axis, bool reverse)
    {
        detail::Guard guard;
        if (running()) {
            return "already running";
        }
        g_axis = axis;
        g_reverse = reverse;
        g_goPending = true;
        const char *err = select(Id::Probe);
        if (!err) {
            err = start();
        }
        g_goPending = false;
        return err;
    }

    // ISR context (ProbeInput's GPIO callback) -- no mode lock.
    void probeEdge(bool contactNow)
    {
        if (!g_watching || !contactNow) {
            return;
        }
        int64_t pos = Stepper::haltNow(g_axis);
        g_latchedPos = pos;
        g_watching = false;
        g_contactLatched = true;
    }
}
