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
#include "Jog.hpp"
#include "JogRatio.hpp"
#include "Handwheel.hpp"
#include "Mode.hpp"
#include "Settings.hpp"
#include "RunSettings.hpp"
#include "Watchdog.hpp"
#include "config.hpp"
#include <math.h>
#include <zephyr/kernel.h>

namespace
{
    constexpr int32_t POLL_PERIOD_MS = 2;
    // Tolerates one late repeat before the run-ahead target runs out.
    constexpr uint32_t HOLD_HORIZON_US = 2 * JOG_REPEAT_INTERVAL_MS * 1000;

    struct AxisJog
    {
        Settings::Axis *cfg;
        int64_t (*handwheelCount)();

        struct k_mutex mutex;
        float resolutionMultiplier;
        int64_t lastHandwheelCount;
        bool lastHandwheelCountValid; // false right after boot/resolution change
    };

    AxisJog jogZ = {&Settings::active.z, Handwheel::z, {}, 1.0f, 0, false};
    AxisJog jogX = {&Settings::active.x, Handwheel::x, {}, 1.0f, 0, false};

    AxisJog &stateOf(Stepper::Axis axis)
    {
        return axis == Stepper::Axis::Z ? jogZ : jogX;
    }

    // Edge-triggered on the handwheel, not absolute-position-triggered: if
    // the handwheel hasn't moved since the last tick, this calls nothing at
    // all -- moveTo() is the one choke point Jog/modes/manual `move` all
    // share, and unconditionally re-asserting a remembered target every
    // tick fights whichever of those actually owns the axis right now.
    // Also means an axis with no handwheel wired up is
    // naturally a no-op forever, not something that needs a separate
    // disable switch.
    void tick(AxisJog &s, Stepper::Axis axis)
    {
        int64_t handwheelCount = s.handwheelCount();

        k_mutex_lock(&s.mutex, K_FOREVER);
        int64_t lastCount = s.lastHandwheelCount;
        bool hadLast = s.lastHandwheelCountValid;
        float resolutionMultiplier = s.resolutionMultiplier;
        s.lastHandwheelCount = handwheelCount;
        s.lastHandwheelCountValid = true;
        k_mutex_unlock(&s.mutex);

        if (!hadLast || handwheelCount == lastCount) {
            return;
        }
        if (Mode::owns(axis)) {
            Mode::stop();
        }

        int64_t target = Jog::jogTarget(Stepper::position(axis), lastCount, handwheelCount, s.cfg->motorSteps,
                                         s.cfg->handwheelPpr, resolutionMultiplier);
        Stepper::moveTo(axis, target);
    }

    void pollLoop(void *, void *, void *)
    {
        int wdt = Watchdog::add("jog");
        while (true) {
            Watchdog::feed(wdt);
            tick(jogZ, Stepper::Axis::Z);
            tick(jogX, Stepper::Axis::X);
            k_sleep(K_MSEC(POLL_PERIOD_MS));
        }
    }

    K_THREAD_STACK_DEFINE(pollStack, 1024);
    struct k_thread pollThreadData;
}

namespace Jog
{
    void begin()
    {
        k_mutex_init(&jogZ.mutex);
        k_mutex_init(&jogX.mutex);
        k_thread_create(&pollThreadData, pollStack, K_THREAD_STACK_SIZEOF(pollStack),
                         pollLoop, nullptr, nullptr, nullptr,
                         K_PRIO_PREEMPT(5), 0, K_NO_WAIT);
    }

    void setResolution(Stepper::Axis axis, float multiplier)
    {
        AxisJog &s = stateOf(axis);
        k_mutex_lock(&s.mutex, K_FOREVER);
        s.resolutionMultiplier = multiplier;
        s.lastHandwheelCountValid = false; // next tick re-snaps instead of applying the old ratio retroactively
        k_mutex_unlock(&s.mutex);
    }

    float resolution(Stepper::Axis axis)
    {
        AxisJog &s = stateOf(axis);
        k_mutex_lock(&s.mutex, K_FOREVER);
        float multiplier = s.resolutionMultiplier;
        k_mutex_unlock(&s.mutex);
        return multiplier;
    }

    const char *tap(Stepper::Axis axis, int direction)
    {
        if (Stepper::isDisabled(axis)) {
            return "axis disabled";
        }
        direction = direction > 0 ? 1 : -1;
        int32_t stepDu = RunSettings::jogStepDu();
        if (Mode::owns(axis)) {
            Mode::stop();
        }
        const Settings::Axis &cfg = *stateOf(axis).cfg;
        int64_t steps = llround((double)stepDu * cfg.motorSteps / cfg.screwPitchDu);
        if (steps < 1) {
            steps = 1;
        }
        Stepper::moveBy(axis, direction * steps);
        return nullptr;
    }

    const char *hold(Stepper::Axis axis, int direction)
    {
        if (Stepper::isDisabled(axis)) {
            return "axis disabled";
        }
        direction = direction > 0 ? 1 : -1;
        if (Mode::owns(axis)) {
            Mode::stop();
        }
        Stepper::runAhead(axis, direction, HOLD_HORIZON_US);
        return nullptr;
    }
}
