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
#include "Machine.hpp"
#include <zephyr/kernel.h>

namespace
{
    constexpr size_t AXES = 2;

    struct k_spinlock g_lock;
    int64_t g_offsets[Wcs::COUNT][AXES];
    size_t g_active = 0;
}

namespace Wcs
{
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

    void setOffset(size_t index, Stepper::Axis axis, int64_t du)
    {
        k_spinlock_key_t key = k_spin_lock(&g_lock);
        g_offsets[index][(size_t)axis] = du;
        k_spin_unlock(&g_lock, key);
    }

    int64_t position(Stepper::Axis axis)
    {
        return Machine::position(axis) - offset(active(), axis);
    }

    int64_t toMachine(Stepper::Axis axis, int64_t workDu)
    {
        return workDu + offset(active(), axis);
    }

    void setPosition(Stepper::Axis axis, int64_t workDu)
    {
        setOffset(active(), axis, Machine::position(axis) - workDu);
    }
}
