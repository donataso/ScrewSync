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
#include "Machine.hpp"
#include "EstopInput.hpp"
#include "Events.hpp"
#include "Mode.hpp"
#include "Homing.hpp"
#include "Motion.hpp"
#include "Settings.hpp"
#include <math.h>
#include <zephyr/drivers/hwinfo.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(machine, LOG_LEVEL_INF);

namespace
{
    struct k_mutex g_mutex;
    bool g_estop = true;
    bool g_flashing = false;
    bool g_spinning = false;
    bool g_modeEngagedZ = false;
    bool g_modeEngagedX = false;
    struct k_spinlock g_originLock;
    int64_t g_origin[2];

    int64_t originOf(Stepper::Axis axis)
    {
        k_spinlock_key_t key = k_spin_lock(&g_originLock);
        int64_t du = g_origin[(size_t)axis];
        k_spin_unlock(&g_originLock, key);
        return du;
    }

    void tripEstop(struct k_work *)
    {
        k_mutex_lock(&g_mutex, K_FOREVER);
        g_estop = true;
        k_mutex_unlock(&g_mutex);
        Mode::stop();
        Stepper::setDisabled(Stepper::Axis::Z, true);
        Stepper::setDisabled(Stepper::Axis::X, true);
        Homing::clearHomed(Stepper::Axis::Z);
        Homing::clearHomed(Stepper::Axis::X);
        LOG_ERR("machine: estop tripped");
        Events::Estop msg = {true};
        Events::publish(&ESTOP_CHAN, &msg);
    }
    K_WORK_DEFINE(g_estopWork, tripEstop);

    Machine::State stateLocked()
    {
        if (g_flashing) {
            return Machine::State::Flashing;
        }
        if (g_estop) {
            return Machine::State::EStop;
        }
        if (g_spinning || g_modeEngagedZ || g_modeEngagedX) {
            return Machine::State::Running;
        }
        return Machine::State::Idle;
    }
}

namespace Machine
{
    void begin()
    {
        k_mutex_init(&g_mutex);
    }

    void setSpinning(bool spinning)
    {
        k_mutex_lock(&g_mutex, K_FOREVER);
        g_spinning = spinning;
        k_mutex_unlock(&g_mutex);
    }

    void setModeEngaged(Stepper::Axis axis, bool engaged)
    {
        k_mutex_lock(&g_mutex, K_FOREVER);
        (axis == Stepper::Axis::Z ? g_modeEngagedZ : g_modeEngagedX) = engaged;
        k_mutex_unlock(&g_mutex);
    }

    void estopNow()
    {
        Stepper::haltNow(Stepper::Axis::Z);
        Stepper::haltNow(Stepper::Axis::X);
        k_work_submit(&g_estopWork);
    }

    bool setFlashing(bool flashing)
    {
        k_mutex_lock(&g_mutex, K_FOREVER);
        if (flashing) {
            if (g_spinning || g_modeEngagedZ || g_modeEngagedX) {
                k_mutex_unlock(&g_mutex);
                estopNow();
                return false;
            }
            g_flashing = true;
            k_mutex_unlock(&g_mutex);
            return true;
        }
        g_flashing = false;
        k_mutex_unlock(&g_mutex);
        return true;
    }

    State state()
    {
        k_mutex_lock(&g_mutex, K_FOREVER);
        State s = stateLocked();
        k_mutex_unlock(&g_mutex);
        return s;
    }

    const char *stateName()
    {
        switch (state()) {
        case State::Running:
            return "running";
        case State::Flashing:
            return "flashing";
        case State::EStop:
            return "estop";
        default:
            return "idle";
        }
    }

    bool blocksTraffic()
    {
        State s = state();
        return s == State::Flashing;
    }

    bool allowsUpdate()
    {
        State s = state();
        return s == State::Idle || s == State::Flashing || s == State::EStop;
    }

    bool estopOff()
    {
        if (EstopInput::active()) {
            return false;
        }
        k_mutex_lock(&g_mutex, K_FOREVER);
        g_estop = false;
        k_mutex_unlock(&g_mutex);
        Events::Estop msg = {false};
        Events::publish(&ESTOP_CHAN, &msg);
        return true;
    }

    void applyBootState()
    {
        uint32_t cause = 0;
        if (hwinfo_get_reset_cause(&cause) != 0 || !(cause & (RESET_POR | RESET_PIN))) {
            return;
        }
        int32_t bootState = Settings::g_active.bootState;
        if (bootState < 1 || !estopOff()) {
            return;
        }
        if (bootState == 2) {
            Motion::setAxisEnabled(Stepper::Axis::Z, true);
            Motion::setAxisEnabled(Stepper::Axis::X, true);
        }
    }

    int64_t position(Stepper::Axis axis)
    {
        return positionOf(axis, Stepper::position(axis));
    }

    int64_t positionOf(Stepper::Axis axis, int64_t steps)
    {
        return llround(Motion::stepsToDu(axis, steps)) - originOf(axis);
    }

    int64_t toSteps(Stepper::Axis axis, int64_t machineDu)
    {
        return Motion::duToSteps(axis, (double)(machineDu + originOf(axis)));
    }

    void setOrigin(Stepper::Axis axis, int64_t du)
    {
        k_spinlock_key_t key = k_spin_lock(&g_originLock);
        g_origin[(size_t)axis] = du;
        k_spin_unlock(&g_originLock, key);
    }
}
