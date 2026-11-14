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
#include "BacklashCycle.hpp"
#include "ModeInternal.hpp"
#include "ProbeCycle.hpp"
#include "ProbeInput.hpp"
#include "Settings.hpp"
#include "Wcs.hpp"
#include <math.h>
#include <stddef.h>
#include <zephyr/sys/util.h>

namespace
{
    using namespace Mode::Detail;

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
    bool g_backlash = false;
    int g_backlashRuns = 0;

    ProbeCycle::State g_cycle;
    BacklashCycle::State g_backlashCycle;
    volatile int32_t g_backlashDu = -1;
    int64_t g_commanded; // re-issuing an unchanged target would undo a contact halt

    // Shared with Mode::probeEdge() (ISR context): plain flags, no mode
    // lock -- the lock is a k_mutex, not ISR-safe.
    volatile BacklashCycle::Watch g_watch = BacklashCycle::Watch::None;
    volatile bool g_contactLatched = false;
    volatile int64_t g_latchedPos = 0;
    volatile int64_t g_latchedMotor = 0;

    int probeDirection()
    {
        return g_reverse ? 1 : -1;
    }

    ProbeCycle::Params cycleParams()
    {
        return {
            probeDirection(),
            duToSteps(g_axis, g_params.probeDstDu),
            duToSteps(g_axis, Settings::g_active.probeBackOffDu),
            duToSteps(g_axis, REPROBE_MARGIN_DU),
            duToSteps(g_axis, g_params.offsetDu),
        };
    }

    BacklashCycle::Params backlashParams()
    {
        return {
            probeDirection(),
            duToSteps(g_axis, g_params.probeDstDu),
            duToSteps(g_axis, Settings::g_active.probeBackOffDu),
            duToSteps(g_axis, REPROBE_MARGIN_DU),
            g_backlashRuns,
        };
    }

    BacklashCycle::Watch probeWatch()
    {
        return ProbeCycle::watching(g_cycle) ? BacklashCycle::Watch::Contact : BacklashCycle::Watch::None;
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
        int64_t pos = Stepper::position(g_axis);
        bool contact = ProbeInput::contact();
        const char *err = g_backlash ? BacklashCycle::begin(g_backlashCycle, backlashParams(), pos, contact)
                                     : ProbeCycle::begin(g_cycle, cycleParams(), pos, contact);
        if (err) {
            return err;
        }
        if (g_backlash) {
            g_backlashDu = -1;
        }
        g_contactLatched = false;
        g_commanded = INT64_MIN;
        g_watch = g_backlash ? BacklashCycle::watching(g_backlashCycle) : probeWatch();
        return nullptr;
    }

    void command(bool reprobeFeed, int64_t target)
    {
        if (target == g_commanded) {
            return;
        }
        g_commanded = target;
        int32_t feedDuPerMin = reprobeFeed ? Settings::g_active.reprobeFeedDuPerMin : Settings::g_active.probeFeedDuPerMin;
        int32_t speed = duPerMinToStepsPerSec(g_axis, feedDuPerMin);
        if (Stepper::moveTo(g_axis, target, speed) != target) {
            g_watch = BacklashCycle::Watch::None;
            finish(Stepper::isDisabled(g_axis) ? "axis disabled" : "travel limit");
        }
    }

    void tickBacklash()
    {
        BacklashCycle::Input in = {Stepper::position(g_axis), Stepper::motorPosition(g_axis), ProbeInput::contact(),
                                   g_contactLatched, g_latchedPos, g_latchedMotor};
        if (in.latched) {
            g_contactLatched = false;
            g_commanded = INT64_MIN;
        }
        BacklashCycle::Result r = BacklashCycle::tick(g_backlashCycle, backlashParams(), in);
        g_watch = BacklashCycle::watching(g_backlashCycle);
        if (r.error) {
            g_watch = BacklashCycle::Watch::None;
            finish(r.error);
            return;
        }
        if (r.done) {
            g_backlashDu = (int32_t)llround(stepsToDu(g_axis, r.backlashSteps));
            g_watch = BacklashCycle::Watch::None;
            finish();
            return;
        }
        if (r.move) {
            command(r.reprobeFeed, r.target);
        }
    }

    void tickMode(int64_t)
    {
        if (g_backlash) {
            tickBacklash();
            return;
        }
        int64_t currentPos = Stepper::position(g_axis);
        bool contactNow = ProbeInput::contact();
        bool justContacted = g_contactLatched;
        int64_t contactPos = g_latchedPos;
        if (justContacted) {
            g_contactLatched = false;
            g_commanded = INT64_MIN;
        }

        ProbeCycle::Result r = ProbeCycle::tick(g_cycle, cycleParams(), currentPos, contactNow, justContacted, contactPos);
        g_watch = probeWatch();

        if (r.error) {
            g_watch = BacklashCycle::Watch::None;
            finish(r.error);
            return;
        }
        if (r.done) {
            Wcs::setOffset(Wcs::active(), g_axis, llround(stepsToDu(g_axis, r.surfaceSteps)));
            g_watch = BacklashCycle::Watch::None;
            finish();
            return;
        }
        if (r.move) {
            command(r.reprobeFeed, r.target);
        }
    }

    void progressBacklash(Mode::Progress &out)
    {
        switch (g_backlashCycle.phase) {
        case BacklashCycle::Phase::Approach:
            out.phase = "approach";
            break;
        case BacklashCycle::Phase::Release:
            out.phase = "release";
            break;
        case BacklashCycle::Phase::BackOff:
            out.phase = "back off";
            break;
        default:
            out.phase = "run";
            break;
        }
        out.pass = g_backlashCycle.run + 1 > g_backlashRuns ? g_backlashRuns : g_backlashCycle.run + 1;
        out.passes = g_backlashRuns;
    }

    void progressMode(Mode::Progress &out)
    {
        if (g_backlash) {
            progressBacklash(out);
            return;
        }
        switch (g_cycle.phase) {
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

namespace Mode::Detail
{
    const Ops PROBE = {true, &g_params, sizeof(g_params), PARAMS, ARRAY_SIZE(PARAMS), startMode, tickMode,
                       nullptr, nullptr, progressMode, nullptr, nullptr, nullptr};
}

namespace Mode
{
    const char *probeGo(Stepper::Axis axis, bool reverse)
    {
        Detail::Guard guard;
        if (running()) {
            return "already running";
        }
        g_axis = axis;
        g_reverse = reverse;
        g_backlash = false;
        g_goPending = true;
        const char *err = select(Id::Probe);
        if (!err) {
            err = start();
        }
        g_goPending = false;
        return err;
    }

    const char *probeBacklash(Stepper::Axis axis, bool reverse, int runs)
    {
        Detail::Guard guard;
        if (running()) {
            return "already running";
        }
        if (runs < 1 || runs > BacklashCycle::MAX_RUNS) {
            return "bad run count";
        }
        g_axis = axis;
        g_reverse = reverse;
        g_backlash = true;
        g_backlashRuns = runs;
        g_goPending = true;
        const char *err = select(Id::Probe);
        if (!err) {
            err = start();
        }
        g_goPending = false;
        return err;
    }

    int32_t lastBacklashDu()
    {
        return g_backlashDu;
    }

    // ISR context (ProbeInput's GPIO callback) -- no mode lock.
    void probeEdge(bool contactNow)
    {
        BacklashCycle::Watch watch = g_watch;
        bool wanted = contactNow ? watch == BacklashCycle::Watch::Contact : watch == BacklashCycle::Watch::Release;
        if (!wanted) {
            return;
        }
        g_latchedPos = Stepper::haltNow(g_axis);
        g_latchedMotor = Stepper::motorPosition(g_axis);
        g_watch = BacklashCycle::Watch::None;
        g_contactLatched = true;
    }
}
