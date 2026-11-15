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
#include "RunSettings.hpp"
#include "Encoder.hpp"
#include "Events.hpp"
#include "Machine.hpp"
#include "Watchdog.hpp"
#include "ModeSteps.hpp"
#include "Wcs.hpp"
#include <stdio.h>
#include <string.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <initializer_list>

LOG_MODULE_REGISTER(mode, LOG_LEVEL_INF);

namespace
{
    using Mode::Detail::Ops;

    constexpr int32_t TICK_PERIOD_MS = 2;

    const char *const NAMES[Mode::COUNT] = {
        "gear",
        "async",
        "turn",
        "thread",
        "part",
        "slot",
        "ellipse",
        "gcode",
        "probe",
    };

    const Ops *const OPS[Mode::COUNT] = {
        &Mode::Detail::GEAR,
        &Mode::Detail::ASYNC,
        &Mode::Detail::TURN,
        &Mode::Detail::THREAD,
        &Mode::Detail::PART,
        &Mode::Detail::SLOT,
        &Mode::Detail::ELLIPSE,
        &Mode::Detail::GCODE,
        nullptr,
    };

    K_MUTEX_DEFINE(g_lock);
    const Ops *g_active = nullptr;
    Mode::Id g_activeId = Mode::Id::Gear;
    bool g_ownedZ = false;
    bool g_ownedX = false;
    const char *g_lastError = "";
    bool g_spindleReverse = false;
    int32_t g_overridePct = 100;

    // start zero: o<zero> runs first, then this mode starts.
    constexpr const char *ZERO_CALL = "o<zero> call [1] [1]";
    bool g_startAfter = false;
    Mode::Id g_startAfterId = Mode::Id::Gear;

    bool &ownedFlag(Stepper::Axis axis)
    {
        return axis == Stepper::Axis::Z ? g_ownedZ : g_ownedX;
    }

    using Mode::Detail::releaseAxes;

    void publishRunning(bool running)
    {
        Events::Running msg = {running};
        Events::publish(&RUNNING_CHAN, &msg);
    }

    void releaseAll()
    {
        releaseAxes();
        bool wasRunning = g_active != nullptr;
        g_active = nullptr;
        g_startAfter = false;
        if (wasRunning) {
            publishRunning(false);
        }
    }

    const char *startOps(Mode::Id id, const Ops *ops)
    {
        if (g_active) {
            return g_active->resume && g_active->resume() ? nullptr : "already running";
        }
        if (Mode::Detail::mdiActive()) {
            return "MDI running";
        }
        if (!ops) {
            return "mode not implemented";
        }
        const Mode::Detail::Param *missing = Mode::Detail::firstMissing(*ops);
        if (missing) {
            static char reason[32];
            snprintf(reason, sizeof(reason), "set %s first", missing->name);
            return reason;
        }
        g_activeId = id;
        g_spindleReverse = ops->spindleReverse && *ops->spindleReverse;
        g_overridePct = 100;
        const char *refusal = ops->start(Encoder::filteredPosition());
        if (refusal) {
            releaseAll();
            return refusal;
        }
        g_active = ops;
        g_lastError = "";
        publishRunning(true);
        return nullptr;
    }

    K_THREAD_STACK_DEFINE(g_runnerStack, 4096);
    struct k_thread g_runnerThread;

    void runnerLoop(void *, void *, void *)
    {
        int wdt = Watchdog::add("mode");
        while (true) {
            Watchdog::feed(wdt);
            k_mutex_lock(&g_lock, K_FOREVER);
            if (g_active) {
                int64_t spindlePos = Encoder::filteredPosition();
                Encoder::Direction wrong = g_spindleReverse ? Encoder::Direction::Forward : Encoder::Direction::Reverse;
                if (g_active->spindleReverse != nullptr && Encoder::direction() == wrong) {
                    Mode::Detail::finish("wrong spindle direction");
                } else {
                    g_active->tick(spindlePos);
                }
            } else if (Mode::Detail::mdiActive()) {
                Mode::Detail::mdiTick(Encoder::filteredPosition());
            }
            k_mutex_unlock(&g_lock);
            k_sleep(K_MSEC(TICK_PERIOD_MS));
        }
    }
}

namespace Mode::Detail
{
    void lock()
    {
        k_mutex_lock(&g_lock, K_FOREVER);
    }
    void unlock()
    {
        k_mutex_unlock(&g_lock);
    }

    const Ops *ops(Id id)
    {
        return OPS[(size_t)id];
    }

    int32_t overridden(int32_t feed)
    {
        return (int32_t)((int64_t)feed * g_overridePct / 100);
    }

    int32_t feedOverridePct()
    {
        return g_overridePct;
    }

    int spindleSign()
    {
        return g_active && g_spindleReverse ? -1 : 1;
    }

    bool isRunning(Id id)
    {
        return g_active && g_activeId == id;
    }

    void saveParams(Id id)
    {
        const Ops *o = ops(id);
        if (o) {
            RunSettings::saveModeParams(NAMES[(size_t)id], o->params, o->paramsSize);
        }
    }

    void releaseAxes()
    {
        probeDisarm();
        for (Stepper::Axis axis : {Stepper::Axis::Z, Stepper::Axis::X}) {
            if (ownedFlag(axis)) {
                ownedFlag(axis) = false;
                Machine::setModeEngaged(axis, false);
                Stepper::setHoldRequested(axis, false);
            }
        }
    }

    void setLastError(const char *reason)
    {
        g_lastError = reason;
    }

    void resetFeedOverride()
    {
        g_overridePct = 100;
    }

    const char *claim(Stepper::Axis axis)
    {
        if (Stepper::isDisabled(axis)) {
            return "axis disabled";
        }
        ownedFlag(axis) = true;
        Machine::setModeEngaged(axis, true);
        Stepper::setHoldRequested(axis, true);
        return nullptr;
    }

    void finish(const char *reason)
    {
        if (!g_active) {
            return;
        }
        g_lastError = reason ? reason : "";
        if (reason) {
            LOG_WRN("%s stopped: %s", NAMES[(size_t)g_activeId], reason);
            for (Stepper::Axis axis : {Stepper::Axis::Z, Stepper::Axis::X}) {
                Stepper::stop(axis);
            }
        }
        if (!reason && g_startAfter) {
            // Running stays on from o<zero> into the mode.
            Mode::Id next = g_startAfterId;
            releaseAxes();
            g_active = nullptr;
            g_startAfter = false;
            const char *refusal = startOps(next, OPS[(size_t)next]);
            if (refusal) {
                g_lastError = refusal;
                publishRunning(false);
            }
            return;
        }
        releaseAll();
    }
}

namespace Mode
{
    void begin()
    {
        for (size_t i = 0; i < COUNT; i++) {
            if (OPS[i]) {
                RunSettings::loadModeParams(NAMES[i], OPS[i]->params, OPS[i]->paramsSize);
            }
        }
        Detail::mdiBegin();
        k_thread_create(&g_runnerThread, g_runnerStack, K_THREAD_STACK_SIZEOF(g_runnerStack), runnerLoop, nullptr,
                        nullptr, nullptr, K_PRIO_PREEMPT(5), 0, K_NO_WAIT);
    }

    const char *name(Id id)
    {
        return NAMES[(size_t)id];
    }

    bool parse(const char *name, Id &out)
    {
        for (size_t i = 0; i < COUNT; i++) {
            if (!strcmp(name, NAMES[i])) {
                out = (Id)i;
                return true;
            }
        }
        return false;
    }

    Id current()
    {
        return RunSettings::mode();
    }

    bool running()
    {
        Detail::Guard guard;
        return g_active != nullptr;
    }

    bool busy()
    {
        Detail::Guard guard;
        return g_active != nullptr || Detail::mdiActive();
    }

    bool owns(Stepper::Axis axis)
    {
        Detail::Guard guard;
        return ownedFlag(axis);
    }

    const char *select(Id id)
    {
        Detail::Guard guard;
        if (g_active) {
            return "stop first";
        }
        RunSettings::setMode(id);
        return nullptr;
    }

    const char *start(bool fromZero)
    {
        Detail::Guard guard;
        Id id = current();
        const Ops *ops = OPS[(size_t)id];
        if (!fromZero || g_active) {
            return startOps(id, ops);
        }
        if (!ops) {
            return "mode not implemented";
        }
        const Detail::Param *missing = Detail::firstMissing(*ops);
        if (missing) {
            static char reason[32];
            snprintf(reason, sizeof(reason), "set %s first", missing->name);
            return reason;
        }
        Detail::gcodeSetCall(ZERO_CALL);
        const char *err = startOps(Id::Gcode, &Detail::GCODE);
        Detail::gcodeSetCall(nullptr);
        if (!err) {
            g_startAfter = true;
            g_startAfterId = id;
        }
        return err;
    }

    const char *call(const char *line)
    {
        Detail::Guard guard;
        if (g_active) {
            return "already running";
        }
        Detail::gcodeSetCall(line);
        const char *err = startOps(Id::Gcode, &Detail::GCODE);
        Detail::gcodeSetCall(nullptr);
        return err;
    }

    bool gcodeRunning()
    {
        Detail::Guard guard;
        return g_active && g_activeId == Id::Gcode;
    }

    bool hasFeedOverride(Id id)
    {
        return OPS[(size_t)id] && id != Id::Thread;
    }

    bool startsAtZero(Id id)
    {
        return id == Id::Turn || id == Id::Thread || id == Id::Part || id == Id::Slot || id == Id::Ellipse;
    }

    bool confirmsOffZero(Id id)
    {
        return id == Id::Gcode;
    }

    StartPrompt startPrompt()
    {
        bool atZero = Wcs::position(Stepper::Axis::Z) == 0 && Wcs::position(Stepper::Axis::X) == 0;
        if (running() || atZero) {
            return StartPrompt::None;
        }
        Id id = current();
        if (startsAtZero(id)) {
            return StartPrompt::OfferZero;
        }
        return confirmsOffZero(id) ? StartPrompt::ConfirmOffZero : StartPrompt::None;
    }

    bool canSkip(Id id)
    {
        return OPS[(size_t)id] && OPS[(size_t)id]->skip;
    }

    int32_t feedOverride()
    {
        Detail::Guard guard;
        return g_overridePct;
    }

    const char *setFeedOverride(int32_t pct)
    {
        Detail::Guard guard;
        if (!g_active && !Detail::mdiActive()) {
            return "not running";
        }
        if (g_active && !hasFeedOverride(g_activeId)) {
            return "no feed override in this mode";
        }
        g_overridePct = CLAMP(pct, OVERRIDE_MIN_PCT, OVERRIDE_MAX_PCT);
        return nullptr;
    }

    const char *skip()
    {
        Detail::Guard guard;
        if (!g_active) {
            return "not running";
        }
        if (!g_active->skip) {
            return "no passes to skip";
        }
        g_active->skip();
        return nullptr;
    }

    Progress progress()
    {
        Detail::Guard guard;
        Progress p = {"", 0, 0, false, 0};
        if (g_active) {
            p.phase = "run";
            if (g_active->progress) {
                g_active->progress(p);
            }
        }
        return p;
    }

    const char *lastError()
    {
        Detail::Guard guard;
        return g_lastError;
    }

    void stop()
    {
        Detail::Guard guard;
        Detail::mdiClear();
        releaseAll();
        for (Stepper::Axis axis : {Stepper::Axis::Z, Stepper::Axis::X}) {
            Stepper::stop(axis);
        }
    }

    void onAxisDisabled(Stepper::Axis axis)
    {
        Detail::Guard guard;
        if (ownedFlag(axis)) {
            stop();
        }
    }
}
