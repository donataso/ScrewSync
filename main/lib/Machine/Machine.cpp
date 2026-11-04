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
#include "PersistBlob.hpp"
#include <zephyr/drivers/hwinfo.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/reboot.h>

LOG_MODULE_REGISTER(machine, LOG_LEVEL_INF);

namespace
{
    constexpr const char *NVS_NS = "machine";
    constexpr const char *ESTOP_KEY = "estop";

    struct k_mutex mutex;
    bool g_estop = false;
    bool g_flashing = false;
    bool g_spinning = false;
    bool g_modeEngagedZ = false;
    bool g_modeEngagedX = false;

    // Caller holds mutex.
    Machine::State stateLocked()
    {
        if (g_estop) {
            return Machine::State::EStop;
        }
        if (g_flashing) {
            return Machine::State::Flashing;
        }
        if (g_spinning || g_modeEngagedZ || g_modeEngagedX) {
            return Machine::State::Running;
        }
        return Machine::State::Idle;
    }

    // Caller holds mutex. De-energizes both axes so nothing already in
    // motion keeps running once the trip is persisted -- Stepper::moveTo()
    // also refuses new targets while Flashing/EStop (see Stepper.cpp), this
    // just cuts the drivers immediately instead of waiting for their next
    // poll tick.
    void haltAxesLocked()
    {
        Stepper::setEnabled(Stepper::Axis::Z, false);
        Stepper::setEnabled(Stepper::Axis::X, false);
    }

    // Caller holds mutex.
    void tripEstopLocked()
    {
        g_estop = true;
        haltAxesLocked();
        uint8_t v = 1;
        PersistBlob::save(NVS_NS, ESTOP_KEY, &v, sizeof(v));
        LOG_ERR("machine: estop tripped, rebooting");
        sys_reboot(SYS_REBOOT_WARM);
    }
}

namespace Machine
{
    void begin()
    {
        k_mutex_init(&mutex);
        uint8_t v = 0;
        if (PersistBlob::load(NVS_NS, ESTOP_KEY, &v, sizeof(v)) == sizeof(v) && v) {
            g_estop = true;
            LOG_ERR("machine: booted into estop, waiting for ack");
        }
        uint32_t cause = 0;
        if (hwinfo_get_reset_cause(&cause) == 0 && (cause & RESET_WATCHDOG)) {
            g_estop = true;
            LOG_ERR("machine: watchdog reset, waiting for ack");
        }
    }

    void setSpinning(bool spinning)
    {
        k_mutex_lock(&mutex, K_FOREVER);
        g_spinning = spinning;
        k_mutex_unlock(&mutex);
    }

    void setModeEngaged(Stepper::Axis axis, bool engaged)
    {
        k_mutex_lock(&mutex, K_FOREVER);
        (axis == Stepper::Axis::Z ? g_modeEngagedZ : g_modeEngagedX) = engaged;
        k_mutex_unlock(&mutex);
    }

    bool setFlashing(bool flashing)
    {
        k_mutex_lock(&mutex, K_FOREVER);
        if (g_estop) {
            k_mutex_unlock(&mutex);
            return false;
        }
        if (flashing)
        {
            if (g_spinning || g_modeEngagedZ || g_modeEngagedX)
            {
                tripEstopLocked(); // does not return
            }
            g_flashing = true;
            k_mutex_unlock(&mutex);
            return true;
        }
        g_flashing = false;
        k_mutex_unlock(&mutex);
        return true;
    }

    State state()
    {
        k_mutex_lock(&mutex, K_FOREVER);
        State s = stateLocked();
        k_mutex_unlock(&mutex);
        return s;
    }

    const char *stateName()
    {
        switch (state())
        {
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
        return s == State::Flashing || s == State::EStop;
    }

    bool allowsUpdate()
    {
        State s = state();
        return s == State::Idle || s == State::Flashing;
    }

    void ackEstop()
    {
        k_mutex_lock(&mutex, K_FOREVER);
        g_estop = false;
        uint8_t v = 0;
        PersistBlob::save(NVS_NS, ESTOP_KEY, &v, sizeof(v));
        k_mutex_unlock(&mutex);
        sys_reboot(SYS_REBOOT_WARM);
    }
}
