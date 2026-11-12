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
#include "Handwheel.hpp"
#include "Settings.hpp"
#include <zephyr/device.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <hal/pcnt_ll.h>

LOG_MODULE_REGISTER(handwheel, LOG_LEVEL_INF);

// Z/X handwheels are PCNT units 1/2 on the same peripheral as the spindle
// encoder (main/lib/Encoder) -- read by plain polling here rather than
// the spindle's threshold-interrupt scheme, since a hand-cranked handwheel
// can't come close to wrapping the 16-bit counter between poll ticks, and
// this avoids a second unit sharing (and fighting over) the PCNT interrupt.

namespace
{
    constexpr uint32_t UNIT_Z = 1;
    constexpr uint32_t UNIT_X = 2;
    constexpr int32_t POLL_PERIOD_MS = 10;

    const struct device *g_dev = DEVICE_DT_GET(DT_NODELABEL(pcnt));
    pcnt_dev_t *g_hw = PCNT_LL_GET_HW(0);

    struct k_mutex g_mutex;
    int64_t g_accZ = 0, g_accX = 0;
    int16_t g_lastZ = 0, g_lastX = 0;

    K_THREAD_STACK_DEFINE(g_pollStack, 1024);
    struct k_thread g_pollThreadData;

    void pollLoop(void *, void *, void *)
    {
        while (true) {
            int16_t curZ = (int16_t)pcnt_ll_get_count(g_hw, UNIT_Z);
            int16_t curX = (int16_t)pcnt_ll_get_count(g_hw, UNIT_X);

            k_mutex_lock(&g_mutex, K_FOREVER);
            g_accZ += (int16_t)(curZ - g_lastZ);
            g_accX += (int16_t)(curX - g_lastX);
            g_lastZ = curZ;
            g_lastX = curX;
            k_mutex_unlock(&g_mutex);

            k_sleep(K_MSEC(POLL_PERIOD_MS));
        }
    }
}

namespace Handwheel
{
    void begin()
    {
        if (!device_is_ready(g_dev)) {
            LOG_ERR("pcnt device not ready");
            return;
        }

        k_mutex_init(&g_mutex);
        k_thread_create(&g_pollThreadData, g_pollStack, K_THREAD_STACK_SIZEOF(g_pollStack),
                        pollLoop, nullptr, nullptr, nullptr,
                        K_PRIO_PREEMPT(5), 0, K_NO_WAIT);
    }

    int64_t z()
    {
        k_mutex_lock(&g_mutex, K_FOREVER);
        int64_t pos = g_accZ;
        k_mutex_unlock(&g_mutex);
        return Settings::g_active.z.invertHandwheel ? -pos : pos;
    }

    int64_t x()
    {
        k_mutex_lock(&g_mutex, K_FOREVER);
        int64_t pos = g_accX;
        k_mutex_unlock(&g_mutex);
        return Settings::g_active.x.invertHandwheel ? -pos : pos;
    }
}
