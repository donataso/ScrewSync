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
#include "StepperRamp.hpp"
#include <stddef.h>
#include <zephyr/sys/util.h>

namespace
{
    using namespace Mode::Detail;

    struct Async
    {
        int32_t speedDuPerMin;
        bool reverse;
        Stepper::Axis axis;
        int32_t stopAfterDu; // from the engage point, 0 = off
    };

    constexpr double MAX_SPEED_DU_PER_MIN = 60000000;
    constexpr double MAX_LENGTH_DU = 10000000;
    // Look-ahead per tick: motion stops by itself if ticks stop coming.
    constexpr uint32_t HORIZON_US = 100000;

    Async g_params = {0, false, Stepper::Axis::Z, 0};

    const Param PARAMS[] = {
        {"speed", Kind::Du, offsetof(Async, speedDuPerMin), 0, MAX_SPEED_DU_PER_MIN, true, true},
        {"dir", Kind::Flag, offsetof(Async, reverse), 0, 0, false, false, "-|+"},
        {"axis", Kind::Axis, offsetof(Async, axis), 0, 0, false},
        {"stop-after", Kind::Du, offsetof(Async, stopAfterDu), 0, MAX_LENGTH_DU, false},
    };

    int g_direction;
    bool g_hasEnd;
    int64_t g_endSteps;

    const char *startMode(int64_t)
    {
        const char *refusal = claim(g_params.axis);
        if (refusal) {
            return refusal;
        }
        g_direction = feedDirection(signedDu(g_params.speedDuPerMin, g_params.reverse));
        g_hasEnd = g_params.stopAfterDu > 0;
        if (g_hasEnd) {
            g_endSteps = Stepper::position(g_params.axis) + g_direction * duToSteps(g_params.axis, g_params.stopAfterDu);
        }
        return nullptr;
    }

    void tickMode(int64_t)
    {
        Stepper::Axis axis = g_params.axis;
        if (g_params.speedDuPerMin == 0) {
            Stepper::stop(axis);
            return;
        }
        int32_t speed = duPerMinToStepsPerSec(axis, overridden(g_params.speedDuPerMin));
        const Settings::Axis &cfg = axis == Stepper::Axis::Z ? Settings::g_active.z : Settings::g_active.x;
        int64_t target = Stepper::position(axis) +
                         g_direction * Stepper::runAheadDistance(speed, cfg.acceleration, speed, HORIZON_US);
        if (g_hasEnd && (g_direction > 0 ? target > g_endSteps : target < g_endSteps)) {
            target = g_endSteps;
        }
        Stepper::moveTo(axis, target, speed);
        if (g_hasEnd && Stepper::arrived(axis) && Stepper::position(axis) == g_endSteps) {
            finish();
        }
    }
}

namespace Mode::Detail
{
    const Ops ASYNC = {false, &g_params, sizeof(g_params), PARAMS, ARRAY_SIZE(PARAMS), startMode, tickMode,
                       nullptr, nullptr, nullptr, nullptr};
}
