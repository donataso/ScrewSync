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
#include "Settings.hpp"
#include <math.h>
#include <stddef.h>
#include <stdlib.h>
#include <zephyr/sys/util.h>

namespace
{
    using namespace Mode::Detail;

    // Quarter ellipse on the end of the stock, from the face (depth into X)
    // to the diameter (length along Z), engage point = the corner. Passes
    // grow the profile from the corner out.
    struct Ellipse
    {
        int32_t pitchDu; // feed per rev along the profile
        bool reverse;    // profile on the +Z side
        int32_t lengthDu;
        int32_t depthDu;
        int32_t passes;
        bool concave;
        bool spindleReverse;
    };

    constexpr double MAX_PITCH_DU = 1000000;
    constexpr double MAX_LENGTH_DU = 10000000;
    constexpr double MAX_PASSES = 1000;
    constexpr double QUARTER_TURN = 1.57079632679489661923;

    enum class Phase : uint8_t
    {
        Backoff,  // Z clear of the face
        Position, // X to the profile start
        Approach, // Z onto the face
        Cut,
        Retract,  // X clear of the diameter
        HomeX,
        HomeZ,
    };

    Ellipse g_params = {0, false, 0, 0, 5, false, false};

    const Param PARAMS[] = {
        {"pitch", Kind::Du, offsetof(Ellipse, pitchDu), 0, MAX_PITCH_DU, false, true},
        {"dir", Kind::Flag, offsetof(Ellipse, reverse), 0, 0, false, false, "-|+"},
        {"length", Kind::Du, offsetof(Ellipse, lengthDu), 0, MAX_LENGTH_DU, false, true},
        {"depth", Kind::Du, offsetof(Ellipse, depthDu), 0, MAX_LENGTH_DU, false, true},
        {"passes", Kind::Count, offsetof(Ellipse, passes), 1, MAX_PASSES, false},
        {"concave", Kind::Flag, offsetof(Ellipse, concave), 0, 0, false},
        {"spindle", Kind::Flag, offsetof(Ellipse, spindleReverse), 0, 0, false, false, "fwd|rev"},
    };

    int g_zDirection;
    int64_t g_z0;
    int64_t g_x0;
    int32_t g_pass;
    Phase g_phase;
    double g_angle;
    int64_t g_lastSpindle;

    double scale()
    {
        return (double)g_pass / g_params.passes;
    }

    // Profile point at angle, as du offsets from the corner (x <= 0 = into the stock).
    void point(double angle, double &zDu, double &xDu)
    {
        double a = scale() * g_params.lengthDu;
        double b = scale() * g_params.depthDu;
        if (g_params.concave) {
            zDu = a * sin(angle);
            xDu = -b * cos(angle);
        } else {
            zDu = a * (1 - cos(angle));
            xDu = -b * (1 - sin(angle));
        }
    }

    // Profile length per radian at angle, du.
    double speed(double angle)
    {
        double a = scale() * g_params.lengthDu;
        double b = scale() * g_params.depthDu;
        double dz = g_params.concave ? a * cos(angle) : a * sin(angle);
        double dx = g_params.concave ? b * sin(angle) : b * cos(angle);
        double v = sqrt(dz * dz + dx * dx);
        return v < 1 ? 1 : v;
    }

    int64_t zAt(double zDu)
    {
        return g_z0 + g_zDirection * duToSteps(Stepper::Axis::Z, zDu);
    }
    int64_t xAt(double xDu)
    {
        return g_x0 + duToSteps(Stepper::Axis::X, xDu);
    }
    int64_t safe(Stepper::Axis axis)
    {
        return duToSteps(axis, Settings::g_active.safeDistanceDu);
    }

    const char *startMode(int64_t)
    {
        const char *refusal = claim(Stepper::Axis::Z);
        if (!refusal) {
            refusal = claim(Stepper::Axis::X);
        }
        if (refusal) {
            return refusal;
        }
        g_zDirection = feedDirection(signedDu(1, g_params.reverse));
        g_z0 = Stepper::position(Stepper::Axis::Z);
        g_x0 = Stepper::position(Stepper::Axis::X);
        g_pass = 1;
        g_phase = Phase::Backoff;
        return nullptr;
    }

    void tickMode(int64_t spindlePos)
    {
        double zDu;
        double xDu;
        switch (g_phase) {
        case Phase::Backoff:
            if (moveAndWait(Stepper::Axis::Z, g_z0 - g_zDirection * safe(Stepper::Axis::Z))) {
                g_phase = g_pass > g_params.passes ? Phase::HomeX : Phase::Position;
            }
            return;
        case Phase::Position:
            point(0, zDu, xDu);
            if (moveAndWait(Stepper::Axis::X, xAt(xDu))) {
                g_phase = Phase::Approach;
            }
            return;
        case Phase::Approach:
            if (moveAndWait(Stepper::Axis::Z, g_z0)) {
                g_angle = 0;
                g_lastSpindle = spindlePos;
                g_phase = Phase::Cut;
            }
            return;
        case Phase::Cut:
        {
            double revs = (double)spindleTravel(g_lastSpindle, spindlePos) / countsPerRev();
            g_lastSpindle = spindlePos;
            g_angle += revs * abs(overridden(g_params.pitchDu)) / speed(g_angle);
            g_angle = CLAMP(g_angle, 0, QUARTER_TURN);
            point(g_angle, zDu, xDu);
            int64_t z = zAt(zDu);
            int64_t x = xAt(xDu);
            Stepper::moveTo(Stepper::Axis::Z, z, rapidSpeed(Stepper::Axis::Z));
            Stepper::moveTo(Stepper::Axis::X, x, rapidSpeed(Stepper::Axis::X));
            if (g_angle == QUARTER_TURN && Stepper::arrived(Stepper::Axis::Z) && Stepper::arrived(Stepper::Axis::X) &&
                Stepper::position(Stepper::Axis::Z) == z && Stepper::position(Stepper::Axis::X) == x) {
                g_phase = Phase::Retract;
            }
            return;
        }
        case Phase::Retract:
            if (moveAndWait(Stepper::Axis::X, g_x0 + safe(Stepper::Axis::X))) {
                g_pass++;
                g_phase = Phase::Backoff;
            }
            return;
        case Phase::HomeX:
            if (moveAndWait(Stepper::Axis::X, g_x0)) {
                g_phase = Phase::HomeZ;
            }
            return;
        case Phase::HomeZ:
            if (moveAndWait(Stepper::Axis::Z, g_z0)) {
                finish();
            }
            return;
        }
    }

    void progressMode(Mode::Progress &out)
    {
        static const char *const NAMES[] = {"backoff", "position", "approach", "cut", "retract", "home", "home"};
        out.phase = NAMES[(size_t)g_phase];
        out.pass = g_pass > g_params.passes ? g_params.passes : g_pass;
        out.passes = g_params.passes;
    }

    void skipMode()
    {
        if (g_pass < g_params.passes && (g_phase == Phase::Retract || g_phase == Phase::Backoff)) {
            g_pass++;
        }
    }
}

namespace Mode::Detail
{
    const Ops ELLIPSE = {true, &g_params, sizeof(g_params), PARAMS, ARRAY_SIZE(PARAMS), startMode, tickMode,
                         nullptr, nullptr, progressMode, skipMode, &g_params.spindleReverse};
}
