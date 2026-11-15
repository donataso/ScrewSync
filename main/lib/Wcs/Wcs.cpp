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
#include "Wcs.hpp"
#include "HomeInput.hpp"
#include "Machine.hpp"
#include "PersistBlob.hpp"
#include <string.h>
#include <zephyr/kernel.h>

namespace
{
    constexpr size_t AXES = 2;
    constexpr const char *NVS_NS = "wcs";
    constexpr const char *NVS_KEY = "offsets";

    struct k_spinlock g_lock;
    int64_t g_offsets[Wcs::COUNT][AXES];
    size_t g_active = 0;

    void save()
    {
        k_spinlock_key_t key = k_spin_lock(&g_lock);
        int64_t copy[Wcs::COUNT][AXES];
        memcpy(copy, g_offsets, sizeof(copy));
        k_spin_unlock(&g_lock, key);
        PersistBlob::save(NVS_NS, NVS_KEY, copy, sizeof(copy));
    }
}

namespace Wcs
{
    void begin()
    {
        if (!HomeInput::sensors()) {
            return;
        }
        int64_t loaded[COUNT][AXES];
        if (PersistBlob::load(NVS_NS, NVS_KEY, loaded, sizeof(loaded)) == sizeof(loaded)) {
            memcpy(g_offsets, loaded, sizeof(loaded));
        }
    }

    size_t active()
    {
        return g_active;
    }

    int64_t offset(size_t index, Stepper::Axis axis)
    {
        k_spinlock_key_t key = k_spin_lock(&g_lock);
        int64_t du = g_offsets[index][(size_t)axis];
        k_spin_unlock(&g_lock, key);
        return du;
    }

    const char *select(size_t index)
    {
        const char *reason = refusal(index, HomeInput::sensors());
        if (!reason) {
            g_active = index;
        }
        return reason;
    }

    int64_t position(Stepper::Axis axis)
    {
        return Machine::position(axis) - offset(active(), axis);
    }

    int64_t toMachine(Stepper::Axis axis, int64_t workDu)
    {
        return workDu + offset(active(), axis);
    }

    const char *setPosition(size_t index, Stepper::Axis axis, int64_t workDu)
    {
        bool sensors = HomeInput::sensors();
        const char *reason = refusal(index, sensors);
        if (reason) {
            return reason;
        }
        k_spinlock_key_t key = k_spin_lock(&g_lock);
        g_offsets[index][(size_t)axis] = Machine::position(axis) - workDu;
        k_spin_unlock(&g_lock, key);
        if (sensors) {
            save();
        }
        return nullptr;
    }
}
