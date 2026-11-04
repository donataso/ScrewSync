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
#include "Machine.hpp"
#include "Watchdog.hpp"
#include <stdio.h>
#include <string.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <initializer_list>

LOG_MODULE_REGISTER(mode, LOG_LEVEL_INF);

namespace
{
    using Mode::detail::Ops;

    constexpr int32_t TICK_PERIOD_MS = 2;

    const char *const NAMES[Mode::COUNT] = {
        "gear", "async", "turn", "thread", "part", "slot", "ellipse", "gcode", "probe",
    };

    const Ops *const OPS[Mode::COUNT] = {
        &Mode::detail::GEAR, &Mode::detail::ASYNC,   &Mode::detail::TURN,    &Mode::detail::THREAD,
        &Mode::detail::PART, &Mode::detail::SLOT,    &Mode::detail::ELLIPSE, &Mode::detail::GCODE,
        &Mode::detail::PROBE,
    };

    K_MUTEX_DEFINE(g_lock);
    const Ops *g_active = nullptr;
    Mode::Id g_activeId = Mode::Id::Gear;
    bool g_ownedZ = false;
    bool g_ownedX = false;
    const char *g_lastError = "";
    bool g_spindleReverse = false;
    int32_t g_overridePct = 100;

    // Rapid to 0,0, Z first, before the op starts there.
    enum class Homing : uint8_t
    {
        None,
        Z,
        X,
    };
    Homing g_homing = Homing::None;

    bool &ownedFlag(Stepper::Axis axis)
    {
        return axis == Stepper::Axis::Z ? g_ownedZ : g_ownedX;
    }

    void releaseAxes()
    {
        for (Stepper::Axis axis : {Stepper::Axis::Z, Stepper::Axis::X})
        {
            if (ownedFlag(axis)) {
                ownedFlag(axis) = false;
                Machine::setModeEngaged(axis, false);
                Stepper::setHoldRequested(axis, false);
            }
        }
    }

    void releaseAll()
    {
        releaseAxes();
        g_active = nullptr;
        g_homing = Homing::None;
    }

    void tickHoming(int64_t spindlePos)
    {
        if (g_homing == Homing::Z) {
            if (Mode::detail::moveAndWait(Stepper::Axis::Z, 0)) {
                g_homing = Homing::X;
            }
            return;
        }
        if (!Mode::detail::moveAndWait(Stepper::Axis::X, 0)) {
            return;
        }
        g_homing = Homing::None;
        releaseAxes();
        const char *refusal = g_active->start(spindlePos);
        if (refusal) {
            Mode::detail::finish(refusal);
        }
    }

    K_THREAD_STACK_DEFINE(runnerStack, 2048);
    struct k_thread runnerThread;

    void runnerLoop(void *, void *, void *)
    {
        int wdt = Watchdog::add("mode");
        while (true) {
            Watchdog::feed(wdt);
            k_mutex_lock(&g_lock, K_FOREVER);
            if (g_active && g_homing != Homing::None) {
                tickHoming(Encoder::filteredPosition());
            } else if (g_active) {
                int64_t spindlePos = Encoder::filteredPosition();
                Encoder::Direction wrong = g_spindleReverse ? Encoder::Direction::Forward : Encoder::Direction::Reverse;
                if (g_active->spindleReverse && Encoder::direction() == wrong) {
                    Mode::detail::finish("wrong spindle direction");
                } else {
                    g_active->tick(spindlePos);
                }
            }
            k_mutex_unlock(&g_lock);
            k_sleep(K_MSEC(TICK_PERIOD_MS));
        }
    }
}

namespace Mode::detail
{
    void lock() { k_mutex_lock(&g_lock, K_FOREVER); }
    void unlock() { k_mutex_unlock(&g_lock); }

    const Ops *ops(Id id) { return OPS[(size_t)id]; }

    int32_t overridden(int32_t feed) { return (int32_t)((int64_t)feed * g_overridePct / 100); }

    int32_t feedOverridePct() { return g_overridePct; }

    int spindleSign()
    {
        return g_spindleReverse ? -1 : 1;
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
        releaseAll();
    }
}

namespace Mode
{
    void begin()
    {
        for (size_t i = 0; i < COUNT; i++)
        {
            if (OPS[i]) {
                RunSettings::loadModeParams(NAMES[i], OPS[i]->params, OPS[i]->paramsSize);
            }
        }
        k_thread_create(&runnerThread, runnerStack, K_THREAD_STACK_SIZEOF(runnerStack), runnerLoop, nullptr,
                        nullptr, nullptr, K_PRIO_PREEMPT(5), 0, K_NO_WAIT);
    }

    const char *name(Id id)
    {
        return NAMES[(size_t)id];
    }

    bool parse(const char *name, Id &out)
    {
        for (size_t i = 0; i < COUNT; i++)
        {
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
        detail::Guard guard;
        return g_active != nullptr;
    }

    bool owns(Stepper::Axis axis)
    {
        detail::Guard guard;
        return ownedFlag(axis);
    }

    const char *select(Id id)
    {
        detail::Guard guard;
        if (g_active) {
            return "stop first";
        }
        RunSettings::setMode(id);
        return nullptr;
    }

    const char *start(bool fromZero)
    {
        detail::Guard guard;
        if (g_active) {
            return g_active->resume && g_active->resume() ? nullptr : "already running";
        }
        Id id = current();
        const Ops *ops = OPS[(size_t)id];
        if (!ops) {
            return "mode not implemented";
        }
        const detail::Param *missing = detail::firstMissing(*ops);
        if (missing) {
            static char reason[32];
            snprintf(reason, sizeof(reason), "set %s first", missing->name);
            return reason;
        }
        g_activeId = id;
        g_spindleReverse = ops->spindleReverse && *ops->spindleReverse;
        g_overridePct = 100;
        const char *refusal = nullptr;
        if (fromZero) {
            refusal = detail::claim(Stepper::Axis::Z);
            if (!refusal) {
                refusal = detail::claim(Stepper::Axis::X);
            }
            g_homing = Homing::Z;
        } else {
            refusal = ops->start(Encoder::filteredPosition());
        }
        if (refusal) {
            releaseAll();
            return refusal;
        }
        g_active = ops;
        g_lastError = "";
        return nullptr;
    }

    bool hasFeedOverride(Id id)
    {
        return OPS[(size_t)id] && id != Id::Thread && id != Id::Probe;
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
        bool atZero = Stepper::position(Stepper::Axis::Z) == 0 && Stepper::position(Stepper::Axis::X) == 0;
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
        detail::Guard guard;
        return g_overridePct;
    }

    const char *setFeedOverride(int32_t pct)
    {
        detail::Guard guard;
        if (!g_active) {
            return "not running";
        }
        if (!hasFeedOverride(g_activeId)) {
            return "no feed override in this mode";
        }
        g_overridePct = pct < OVERRIDE_MIN_PCT ? OVERRIDE_MIN_PCT : pct > OVERRIDE_MAX_PCT ? OVERRIDE_MAX_PCT : pct;
        return nullptr;
    }

    const char *skip()
    {
        detail::Guard guard;
        if (!g_active) {
            return "not running";
        }
        if (!g_active->skip || g_homing != Homing::None) {
            return "no passes to skip";
        }
        g_active->skip();
        return nullptr;
    }

    Progress progress()
    {
        detail::Guard guard;
        Progress p = {"", 0, 0, false, 0};
        if (g_active) {
            p.phase = "run";
            if (g_homing != Homing::None) {
                p.phase = "position";
            } else if (g_active->progress) {
                g_active->progress(p);
            }
        }
        return p;
    }

    const char *lastError()
    {
        detail::Guard guard;
        return g_lastError;
    }

    void stop()
    {
        detail::Guard guard;
        releaseAll();
        for (Stepper::Axis axis : {Stepper::Axis::Z, Stepper::Axis::X}) {
            Stepper::stop(axis);
        }
    }

    void onAxisDisabled(Stepper::Axis axis)
    {
        detail::Guard guard;
        if (ownedFlag(axis)) {
            stop();
        }
    }
}
