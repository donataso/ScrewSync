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
#include "Homing.hpp"
#include "HomeInput.hpp"
#include "HomingSeq.hpp"
#include "Machine.hpp"
#include "ModeInternal.hpp"
#include "Motion.hpp"
#include "Pins.hpp"
#include "StepperLimits.hpp"
#include <math.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(homing, LOG_LEVEL_INF);

namespace
{
    using Stepper::Axis;

    constexpr size_t AXES = 2;

    Axis g_queue[AXES];
    size_t g_count = 0;
    size_t g_index = 0;
    HomingSeq::Sequence g_sequence;

    volatile bool g_active = false;
    volatile bool g_homed[AXES];

    volatile HomingSeq::Watch g_watch = HomingSeq::Watch::None;
    volatile bool g_latched = false;
    volatile int64_t g_latchPosition = 0;
    volatile Axis g_watchedAxis = Axis::Z;

    void estopWork(struct k_work *)
    {
        Machine::tripEstop();
    }
    K_WORK_DEFINE(g_estopWork, estopWork);

    const Settings::Axis &cfgOf(Axis axis)
    {
        return axis == Axis::Z ? Settings::g_active.z : Settings::g_active.x;
    }

    void beginAxis()
    {
        Axis axis = g_queue[g_index];
        const Settings::Axis &cfg = cfgOf(axis);
        int64_t latchBackOff = Motion::duToSteps(axis, cfg.homeLatchBackOffDu);
        HomingSeq::Params params = {
            Stepper::maxTravelSteps(cfg.maxTravelMm, cfg.motorSteps, cfg.screwPitchDu),
            2 * latchBackOff,
            latchBackOff,
            Motion::duToSteps(axis, cfg.homePullOffDu),
            Mode::Detail::duPerMinToStepsPerSec(axis, cfg.homeFastDuPerMin),
            Mode::Detail::duPerMinToStepsPerSec(axis, cfg.homeSlowDuPerMin),
        };
        g_watch = HomingSeq::Watch::None;
        g_latched = false;
        g_watchedAxis = axis;
        g_sequence.begin(params, HomeInput::active(axis));
    }

    void end()
    {
        g_watch = HomingSeq::Watch::None;
        g_active = false;
        Mode::Detail::releaseAxes();
    }

    void clearAllHomed()
    {
        g_homed[0] = false;
        g_homed[1] = false;
    }

    void fail(const char *reason)
    {
        clearAllHomed();
        LOG_WRN("homing stopped: %s", reason);
        Mode::Detail::setLastError(reason);
        Stepper::stop(Axis::Z);
        Stepper::stop(Axis::X);
        end();
    }

    void axisDone(Axis axis)
    {
        Machine::setOrigin(axis, llround(Motion::stepsToDu(axis, Stepper::position(axis))) - cfgOf(axis).homePositionDu);
        g_homed[(size_t)axis] = true;
        if (++g_index == g_count) {
            end();
            return;
        }
        beginAxis();
    }
}

namespace Homing
{
    const char *start()
    {
        Mode::Detail::Guard guard;
        if (!Motion::idle()) {
            return "machine not idle";
        }
        if (!HomeInput::sensors()) {
            return "no home switch";
        }
        g_count = HomingSeq::queue(Pins::count(PinMap::Fn::XMotor) > 0, g_queue);
        for (size_t i = 0; i < g_count; i++) {
            if (!HomeInput::present(g_queue[i])) {
                return "no home switch";
            }
        }
        for (size_t i = 0; i < g_count; i++) {
            const char *refusal = Mode::Detail::claim(g_queue[i]);
            if (refusal) {
                Mode::Detail::releaseAxes();
                return refusal;
            }
        }
        clearAllHomed();
        Mode::Detail::setLastError("");
        g_index = 0;
        beginAxis();
        g_active = true;
        return nullptr;
    }

    bool active()
    {
        return g_active;
    }

    bool homed(Axis axis)
    {
        return g_homed[(size_t)axis];
    }

    void clearHomed(Axis axis)
    {
        g_homed[(size_t)axis] = false;
    }

    void settingsChanged(const Settings::Conf &before, const Settings::Conf &after)
    {
        for (Axis axis : {Axis::Z, Axis::X}) {
            const Settings::Axis &a = axis == Axis::Z ? before.z : before.x;
            const Settings::Axis &b = axis == Axis::Z ? after.z : after.x;
            if (a.screwPitchDu != b.screwPitchDu || a.motorSteps != b.motorSteps || a.invertDirection != b.invertDirection ||
                a.backlashDu != b.backlashDu) {
                clearHomed(axis);
            }
        }
    }

    void tick()
    {
        Axis axis = g_queue[g_index];
        HomingSeq::Input in = {HomeInput::active(axis), g_latched, g_latchPosition, Stepper::arrived(axis),
                               Stepper::position(axis)};
        HomingSeq::Output out = g_sequence.tick(in);
        if (out.error) {
            fail(out.error);
            return;
        }
        if (out.move) {
            g_latched = false;
            g_watch = out.watch;
            Stepper::moveTo(axis, out.target, out.speed);
        }
        if (g_sequence.state() == HomingSeq::State::Done) {
            axisDone(axis);
        }
    }

    void stop()
    {
        if (g_active) {
            clearAllHomed();
        }
        g_watch = HomingSeq::Watch::None;
        g_active = false;
    }

    void onEdge(Axis axis, bool active)
    {
        if (!g_active) {
            return;
        }
        if (axis != g_watchedAxis) {
            if (active) {
                Stepper::haltNow(Axis::Z);
                Stepper::haltNow(Axis::X);
                k_work_submit(&g_estopWork);
            }
            return;
        }
        HomingSeq::Watch watch = g_watch;
        if (watch == HomingSeq::Watch::None || active != (watch == HomingSeq::Watch::Contact)) {
            return;
        }
        g_latchPosition = Stepper::haltNow(axis);
        g_watch = HomingSeq::Watch::None;
        g_latched = true;
    }
}
