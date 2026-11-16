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
#include "ModeMdiQueue.hpp"
#include "ModeSteps.hpp"
#include "GcodeDialects.hpp"
#include "GcodeMdi.hpp"
#include "Homing.hpp"
#include "Wcs.hpp"
#include <string.h>
#include <zephyr/logging/log.h>
#include <initializer_list>

LOG_MODULE_DECLARE(mode, LOG_LEVEL_INF);

namespace
{
    using namespace Mode::Detail;

    struct PendingPrint : Gcode::PrintSink
    {
        char text[MdiQueue::TEXT_MAX];
        void print(const char *line) override
        {
            strncpy(text, line, sizeof(text) - 1);
            text[sizeof(text) - 1] = '\0';
        }
    };

    Gcode::Mdi g_session;
    PendingPrint g_pendingPrint;
    MdiQueue g_queue;
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

    // Marks the axes each motion step moves, starting from z/x in system active, and claims
    // both axes. A disabled axis is not claimed; it is refused if it moves.
    const char *claimMoved(LineSteps &line, int32_t z, int32_t x, uint8_t active)
    {
        for (size_t i = 0; i < line.count; i++) {
            Gcode::Step &s = line.steps[i];
            if (s.kind == Gcode::StepKind::SelectWcs) {
                active = s.wcs;
                z = s.zDu;
                x = s.xDu;
            }
            if (s.kind == Gcode::StepKind::SetPosition && s.wcs == active) {
                z = s.zDu;
                x = s.xDu;
            }
            if (!isMotion(s.kind)) {
                continue;
            }
            s.axes = (s.zDu != z ? Gcode::AXIS_Z : 0) | (s.xDu != x ? Gcode::AXIS_X : 0);
            for (Stepper::Axis axis : AXES) {
                const char *unavailable = (s.axes & bit(axis)) ? Stepper::unavailable(axis) : nullptr;
                if (unavailable) {
                    return unavailable;
                }
                if (!Stepper::isDisabled(axis)) {
                    claim(axis);
                } else if (s.axes & bit(axis)) {
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
        g_queue.pop();
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
        g_session.begin(*Gcode::find("linuxcnc"), 0, 0, params(), &paramSource(), &g_pendingPrint);
    }

    bool mdiActive()
    {
        return g_queue.count() > 0;
    }

    void mdiTick(int64_t spindlePos)
    {
        const Gcode::Step &s = g_queue.head();
        if (s.kind == Gcode::StepKind::Print) {
            printSink().print(g_queue.headText());
            pop();
            if (!g_queue.count()) {
                releaseAxes();
            }
            return;
        }
        if (!isMotion(s.kind)) {
            Gcode::Step step = s;
            pop();
            const char *refusal = applyMachineStep(step);
            if (refusal) {
                fail(refusal);
                return;
            }
            if (!g_queue.count()) {
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
        if (s.kind == Gcode::StepKind::Probe) {
            bool onZ = g_motion.mainAxis == Stepper::Axis::Z;
            int32_t z = (int32_t)Wcs::position(Stepper::Axis::Z);
            int32_t x = (int32_t)Wcs::position(Stepper::Axis::X);
            g_session.probeDone(onZ ? g_motion.probeTripDu : z, onZ ? x : g_motion.probeTripDu, g_motion.probeFound,
                                g_motion.probeMotorDu);
        }
        pop();
        if (!g_queue.count()) {
            releaseAxes();
        }
    }

    void mdiClear()
    {
        g_queue.clear();
        g_moving = false;
    }
}

namespace Mode
{
    int32_t mdiFree()
    {
        return (int32_t)g_queue.free();
    }

    uint32_t mdiDone()
    {
        return g_queue.completed();
    }

    const char *mdi(const char *line, bool ui, MdiAccepted &accepted)
    {
        Detail::Guard guard;
        if (running()) {
            return "mode running, stop first";
        }
        if (Homing::active()) {
            return "homing";
        }
        // The end position of a G38 move is not known until it ends.
        for (size_t i = 0; i < g_queue.count(); i++) {
            if (g_queue.at(i).kind == Gcode::StepKind::Probe) {
                return "probe running";
            }
        }
        // Jog, handwheel, modes and programs move the axes between MDI lines.
        if (!g_queue.count()) {
            g_session.setPosition((int32_t)Wcs::position(Stepper::Axis::Z), (int32_t)Wcs::position(Stepper::Axis::X));
            g_session.setWcs(wcsState());
        }
        Gcode::Mdi saved = g_session;
        LineSteps steps;
        const char *err = g_session.line(line, ui, g_queue.free(), steps);
        if (err) {
            return err;
        }
        for (size_t i = 0; i < steps.count && Homing::blocked(); i++) {
            if (Detail::isMotion(steps.steps[i].kind)) {
                g_session = saved;
                return "not homed";
            }
        }
        err = claimMoved(steps, saved.zDu(), saved.xDu(), saved.wcsActive());
        if (err) {
            g_session = saved;
            if (!g_queue.count()) {
                Detail::releaseAxes();
            }
            return err;
        }
        accepted.done = g_queue.completed();
        if (!g_queue.count() && steps.count) {
            Detail::resetFeedOverride();
            Detail::setLastError("");
        }
        for (size_t i = 0; i < steps.count; i++) {
            if (steps.steps[i].kind == Gcode::StepKind::Print) {
                g_queue.pushPrint(g_pendingPrint.text);
            } else {
                g_queue.push(steps.steps[i]);
            }
        }
        accepted.steps = (uint32_t)steps.count;
        accepted.free = (int32_t)g_queue.free();
        return nullptr;
    }
}
