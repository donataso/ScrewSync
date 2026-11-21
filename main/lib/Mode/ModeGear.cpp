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
#include "Sync.hpp"
#include "SyncRatio.hpp"
#include "Encoder.hpp"
#include "ModeMath.hpp"
#include <math.h>
#include <stddef.h>
#include <zephyr/sys/util.h>

namespace
{
    using namespace Mode::Detail;

    constexpr double MAX_PITCH_DU = 1000000;
    constexpr double MAX_LENGTH_DU = 10000000;

    Mode::Gear g_params = {0, false, Stepper::Axis::Z, 0, 0};

    const Param PARAMS[] = {
        {"pitch", Kind::Du, offsetof(Mode::Gear, pitchDu), 0, MAX_PITCH_DU, true, true},
        {"dir", Kind::Flag, offsetof(Mode::Gear, reverse), 0, 0, true, false, "-|+"},
        {"axis", Kind::Axis, offsetof(Mode::Gear, axis), 0, 0, false},
        {"stop-after", Kind::Du, offsetof(Mode::Gear, stopAfterDu), 0, MAX_LENGTH_DU, false},
        {"taper", Kind::Angle, offsetof(Mode::Gear, taper), -1, 1, false},
    };

    Sync::Link g_link;
    int g_endDirection; // 0 = no end
    int64_t g_endSteps;
    bool g_tapered;
    int64_t g_zStart;
    int64_t g_xStart;
    int32_t g_linkPct;
    // Ramps onto the thread helix; the axis follows it once joined.
    HelixEntry g_entry;
    bool g_joined;

    int32_t signedPitch()
    {
        return signedDu(g_params.pitchDu, g_params.reverse);
    }

    void relink(int32_t dupr, int64_t spindlePos)
    {
        if (g_joined || g_entry.started) {
            g_link.setDupr(dupr, spindlePos);
            g_joined = true;
            return;
        }
        g_link.duprDu = dupr;
        g_entry.phase = helixPhase(g_link, g_link.originAxisPos);
    }

    const char *startMode(int64_t spindlePos)
    {
        const Mode::Gear &g = g_params;
        g_tapered = g.taper != 0;
        if (g_tapered && g.axis != Stepper::Axis::Z) {
            return "taper needs axis z";
        }
        const char *refusal = Stepper::unavailable(g.axis);
        if (!refusal) {
            refusal = claim(g.axis);
        }
        if (!refusal && g_tapered) {
            refusal = Stepper::unavailable(Stepper::Axis::X);
            if (!refusal) {
                refusal = claim(Stepper::Axis::X);
            }
        }
        if (refusal) {
            return refusal;
        }
        g_linkPct = feedOverridePct();
        g_link = {g.axis, overridden(signedPitch()), 1, spindlePos, Stepper::position(g.axis)};
        int64_t phase = helixPhase(g_link, g_link.originAxisPos);
        g_entry.begin(phase, 0);
        // On the helix already, as after zeroing the angle: follow at once.
        g_joined = Sync::nextOnPhase(spindlePos, phase, countsPerRev(), 1) == spindlePos;
        g_endDirection = 0;
        if (g.stopAfterDu > 0) {
            g_endDirection = feedDirection(signedPitch());
            g_endSteps = Stepper::position(g.axis) + g_endDirection * duToSteps(g.axis, g.stopAfterDu);
        }
        g_zStart = Stepper::position(Stepper::Axis::Z);
        g_xStart = Stepper::position(Stepper::Axis::X);
        return nullptr;
    }

    void tickMode(int64_t spindlePos)
    {
        if (g_linkPct != feedOverridePct()) {
            g_linkPct = feedOverridePct();
            relink(overridden(signedPitch()), spindlePos);
        }
        if (!g_joined && g_entry.joined(g_link, spindlePos)) {
            g_joined = true;
        }
        int64_t target = g_joined ? g_link.target(spindlePos) : g_entry.target(g_link, spindlePos);
        if ((g_endDirection > 0 && target > g_endSteps) || (g_endDirection < 0 && target < g_endSteps)) {
            target = g_endSteps;
        }
        Stepper::moveTo(g_link.axis, target, rapidSpeed(g_link.axis));
        if (g_tapered) {
            double towardChuckDu = -stepsToDu(Stepper::Axis::Z, Stepper::position(Stepper::Axis::Z) - g_zStart);
            int64_t x = g_xStart + duToSteps(Stepper::Axis::X, towardChuckDu * (double)g_params.taper / 2);
            Stepper::moveTo(Stepper::Axis::X, x, rapidSpeed(Stepper::Axis::X));
        }
    }

    double leadInMode(double countsPerSec)
    {
        int32_t pitch = isRunning(Mode::Id::Gear) ? overridden(g_params.pitchDu) : g_params.pitchDu;
        Sync::Link link = {g_params.axis, pitch, 1, 0, 0};
        double speed = link.stepsPerCount() * countsPerSec;
        return stepsToDu(g_params.axis, llround(leadInSteps(speed, rampAccel(g_params.axis), 0)));
    }

    const char *onLiveChange(const void *)
    {
        if (g_tapered && (signedPitch() > 0) != (g_link.duprDu > 0)) {
            return "stop first to reverse a taper";
        }
        relink(overridden(signedPitch()), Encoder::filteredPosition());
        return nullptr;
    }
}

namespace Mode::Detail
{
    const Ops GEAR = {false, &g_params, sizeof(g_params), PARAMS, ARRAY_SIZE(PARAMS), startMode, tickMode,
                      onLiveChange, nullptr, nullptr, nullptr, nullptr, nullptr, leadInMode};
}

namespace Mode
{
    Gear gear()
    {
        Detail::Guard guard;
        return g_params;
    }
}
