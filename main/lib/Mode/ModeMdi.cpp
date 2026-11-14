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
#include "ModeSteps.hpp"
#include "GcodeDialects.hpp"
#include "GcodeMdi.hpp"
#include "Wcs.hpp"
#include <zephyr/logging/log.h>
#include <initializer_list>

LOG_MODULE_DECLARE(mode, LOG_LEVEL_INF);

namespace
{
    using namespace Mode::Detail;

    constexpr size_t QUEUE_LEN = 4;

    Gcode::Mdi g_session;
    Gcode::Step g_queue[QUEUE_LEN];
    size_t g_head = 0;
    size_t g_count = 0;
    bool g_moving = false;
    StepMotion g_motion;

    struct LineSteps : Gcode::StepSink
    {
        Gcode::Step steps[Gcode::Mdi::MAX_LINE_STEPS];
        size_t count = 0;
        void emit(const Gcode::Step &step) override
        {
            if (count < Gcode::Mdi::MAX_LINE_STEPS) {
                steps[count++] = step;
            }
        }
    };

    constexpr Stepper::Axis AXES[] = {Stepper::Axis::Z, Stepper::Axis::X};

    uint8_t bit(Stepper::Axis axis)
    {
        return axis == Stepper::Axis::Z ? Gcode::AXIS_Z : Gcode::AXIS_X;
    }

    int32_t &target(Gcode::Step &s, Stepper::Axis axis)
    {
        return axis == Stepper::Axis::Z ? s.zDu : s.xDu;
    }

    // The last queued M100 for the axis turns it on.
    bool enableQueued(Stepper::Axis axis)
    {
        bool on = false;
        for (size_t i = 0; i < g_count; i++) {
            const Gcode::Step &s = g_queue[(g_head + i) % QUEUE_LEN];
            if (s.kind == Gcode::StepKind::Motors && (s.axes & bit(axis))) {
                on = (axis == Stepper::Axis::Z ? s.zDu : s.xDu) != 0;
            }
        }
        return on;
    }

    // Marks the axes each motion step moves, starting from z/x, and claims
    // both axes. A disabled axis is not claimed; it is refused if it moves
    // and no queued M100 turns it on.
    const char *claimMoved(LineSteps &line, int32_t z, int32_t x)
    {
        for (size_t i = 0; i < line.count; i++) {
            Gcode::Step &s = line.steps[i];
            if (s.kind == Gcode::StepKind::SetPosition) {
                z = s.zDu;
                x = s.xDu;
            }
            if (!isMotion(s.kind)) {
                continue;
            }
            s.axes = (s.zDu != z ? Gcode::AXIS_Z : 0) | (s.xDu != x ? Gcode::AXIS_X : 0);
            for (Stepper::Axis axis : AXES) {
                if (!Stepper::isDisabled(axis)) {
                    claim(axis);
                } else if ((s.axes & bit(axis)) && !enableQueued(axis)) {
                    return "axis disabled";
                }
            }
            z = s.zDu;
            x = s.xDu;
        }
        return nullptr;
    }

    // An axis the step does not move stays where it is now.
    const char *beginMotion(const Gcode::Step &queued, int64_t spindlePos)
    {
        Gcode::Step s = queued;
        for (Stepper::Axis axis : AXES) {
            bool moves = s.axes & bit(axis);
            if (!moves) {
                target(s, axis) = (int32_t)Wcs::position(axis);
            }
            const char *refusal = moves || !Stepper::isDisabled(axis) ? claim(axis) : nullptr;
            if (refusal) {
                return refusal;
            }
        }
        g_motion.begin(s, spindlePos);
        return nullptr;
    }

    void pop()
    {
        g_head = (g_head + 1) % QUEUE_LEN;
        g_count--;
        g_moving = false;
    }

    void fail(const char *reason)
    {
        LOG_WRN("mdi stopped: %s", reason);
        setLastError(reason);
        for (Stepper::Axis axis : {Stepper::Axis::Z, Stepper::Axis::X}) {
            Stepper::stop(axis);
        }
        mdiClear();
        releaseAxes();
    }
}

namespace Mode::Detail
{
    void mdiBegin()
    {
        g_session.begin(*Gcode::find("linuxcnc"), 0, 0);
    }

    bool mdiActive()
    {
        return g_count > 0;
    }

    void mdiTick(int64_t spindlePos)
    {
        const Gcode::Step &s = g_queue[g_head];
        if (!isMotion(s.kind)) {
            Gcode::Step step = s;
            pop();
            // M100 off on a held axis stops and clears MDI, as "enable off" does.
            applyMachineStep(step);
            if (!g_count) {
                releaseAxes();
            }
            return;
        }
        const char *error = nullptr;
        if (!g_moving) {
            error = beginMotion(s, spindlePos);
            if (error) {
                fail(error);
                return;
            }
            g_moving = true;
        }
        bool done = g_motion.drive(s, spindlePos, false, error);
        if (error) {
            fail(error);
            return;
        }
        if (!done) {
            return;
        }
        pop();
        if (!g_count) {
            releaseAxes();
        }
    }

    void mdiClear()
    {
        g_head = 0;
        g_count = 0;
        g_moving = false;
    }
}

namespace Mode
{
    const char *mdi(const char *line, bool ui)
    {
        Detail::Guard guard;
        if (running()) {
            return "mode running, stop first";
        }
        // Jog, handwheel, modes and programs move the axes between MDI lines.
        if (!g_count) {
            g_session.setPosition((int32_t)Wcs::position(Stepper::Axis::Z), (int32_t)Wcs::position(Stepper::Axis::X));
        }
        Gcode::Mdi saved = g_session;
        LineSteps steps;
        const char *err = g_session.line(line, ui, QUEUE_LEN - g_count, steps);
        if (err) {
            return err;
        }
        err = claimMoved(steps, saved.zDu(), saved.xDu());
        if (err) {
            g_session = saved;
            if (!g_count) {
                Detail::releaseAxes();
            }
            return err;
        }
        if (!g_count && steps.count) {
            Detail::resetFeedOverride();
        }
        for (size_t i = 0; i < steps.count; i++) {
            g_queue[(g_head + g_count) % QUEUE_LEN] = steps.steps[i];
            g_count++;
        }
        return nullptr;
    }
}
