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
#include "InputSampler.hpp"
#include "PinMap.hpp"
#include "Pins.hpp"
#include <zephyr/kernel.h>

namespace
{
    constexpr int32_t SAMPLE_PERIOD_MS = 20;
    constexpr uint32_t MIN_PUBLISH_INTERVAL_MS = 100;

    uint8_t g_gpios[PinMap::MAX_GPIO + 1];
    size_t g_count = 0;
    uint64_t g_sampled = 0;
    uint64_t g_published = 0;
    uint32_t g_publishedMs = 0;
    InputSampler::ChangeCallback g_callback = nullptr;
    struct k_spinlock g_lock;
    struct k_work_delayable g_work;

    uint64_t sample()
    {
        uint64_t levels = 0;
        for (size_t i = 0; i < g_count; i++) {
            levels |= (uint64_t)(Pins::readRaw(g_gpios[i]) > 0) << g_gpios[i];
        }
        return levels;
    }

    void tick(struct k_work *)
    {
        uint64_t levels = sample();
        uint32_t now = k_uptime_get_32();
        bool publish = false;
        k_spinlock_key_t key = k_spin_lock(&g_lock);
        if (levels != g_published && now - g_publishedMs >= MIN_PUBLISH_INTERVAL_MS) {
            g_published = levels;
            g_publishedMs = now;
            publish = true;
        }
        k_spin_unlock(&g_lock, key);
        if (publish && g_callback) {
            g_callback();
        }
        k_work_reschedule(&g_work, K_MSEC(SAMPLE_PERIOD_MS));
    }
}

namespace InputSampler
{
    void begin()
    {
        g_count = Pins::sampledGpios(g_gpios, sizeof(g_gpios));
        for (size_t i = 0; i < g_count; i++) {
            g_sampled |= (uint64_t)1 << g_gpios[i];
        }
        k_work_init_delayable(&g_work, tick);
    }

    void start()
    {
        uint64_t levels = sample();
        k_spinlock_key_t key = k_spin_lock(&g_lock);
        g_published = levels;
        g_publishedMs = k_uptime_get_32();
        k_spin_unlock(&g_lock, key);
        if (g_callback) {
            g_callback();
        }
        k_work_reschedule(&g_work, K_MSEC(SAMPLE_PERIOD_MS));
    }

    void stop()
    {
        k_work_cancel_delayable(&g_work);
        k_spinlock_key_t key = k_spin_lock(&g_lock);
        g_published = 0;
        k_spin_unlock(&g_lock, key);
    }

    void onChange(ChangeCallback callback)
    {
        g_callback = callback;
    }

    void published(uint64_t &sampled, uint64_t &levels)
    {
        k_spinlock_key_t key = k_spin_lock(&g_lock);
        sampled = g_sampled;
        levels = g_published;
        k_spin_unlock(&g_lock, key);
    }
}
