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
#include "Watchdog.hpp"
#include "Stepper.hpp"
#include <zephyr/device.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/task_wdt/task_wdt.h>

LOG_MODULE_REGISTER(watchdog, LOG_LEVEL_INF);

namespace
{
    constexpr int64_t FEED_INTERVAL_MS = CONFIG_TASK_WDT_MIN_TIMEOUT / 10;

    const char *g_names[CONFIG_TASK_WDT_CHANNELS];
    int64_t g_lastFeed[CONFIG_TASK_WDT_CHANNELS];
    volatile bool g_tripped = false;

    // Timer ISR. Feeding stops for good, so the hardware watchdog resets the chip.
    void onExpired(int channel, void *)
    {
        g_tripped = true;
        Stepper::setEnabled(Stepper::Axis::Z, false);
        Stepper::setEnabled(Stepper::Axis::X, false);
        LOG_ERR("watchdog: %s stalled", g_names[channel]);
    }
}

namespace Watchdog
{
    void begin()
    {
        const struct device *hw = DEVICE_DT_GET(DT_ALIAS(watchdog0));
        if (!device_is_ready(hw)) {
            LOG_ERR("watchdog: hardware watchdog not ready");
            hw = nullptr;
        }
        int err = task_wdt_init(hw);
        if (err) {
            LOG_ERR("watchdog: init failed (%d)", err);
        }
    }

    int add(const char *name)
    {
        int channel = task_wdt_add(CONFIG_TASK_WDT_MIN_TIMEOUT, onExpired, nullptr);
        if (channel < 0) {
            LOG_ERR("watchdog: cannot add %s (%d)", name, channel);
            return -1;
        }
        g_names[channel] = name;
        g_lastFeed[channel] = k_uptime_get();
        return channel;
    }

    void feed(int channel)
    {
        if (channel < 0 || g_tripped) {
            return;
        }
        int64_t now = k_uptime_get();
        if (now - g_lastFeed[channel] < FEED_INTERVAL_MS) {
            return;
        }
        g_lastFeed[channel] = now;
        task_wdt_feed(channel);
    }
}
