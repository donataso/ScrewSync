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
#include "Mode.hpp"
#include "Motion.hpp"
#include <math.h>
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

    void tripEstop()
    {
        k_mutex_lock(&g_mutex, K_FOREVER);
        g_estop = true;
        k_mutex_unlock(&g_mutex);
        Mode::stop();
        Stepper::setDisabled(Stepper::Axis::Z, true);
        Stepper::setDisabled(Stepper::Axis::X, true);
        LOG_ERR("machine: estop tripped");
    }

    bool setFlashing(bool flashing)
    {
        k_mutex_lock(&g_mutex, K_FOREVER);
        if (flashing) {
            if (g_spinning || g_modeEngagedZ || g_modeEngagedX) {
                k_mutex_unlock(&g_mutex);
                tripEstop();
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

    void estopOff()
    {
        k_mutex_lock(&g_mutex, K_FOREVER);
        g_estop = false;
        k_mutex_unlock(&g_mutex);
    }

    int64_t position(Stepper::Axis axis)
    {
        return llround(Motion::stepsToDu(axis, Stepper::position(axis)));
    }
}
