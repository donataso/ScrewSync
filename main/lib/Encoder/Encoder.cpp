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
#include "Encoder.hpp"
#include "Settings.hpp"
#include <zephyr/device.h>
#include <zephyr/drivers/sensor.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/atomic.h>

LOG_MODULE_REGISTER(encoder, LOG_LEVEL_INF);

// Zephyr's pcnt_esp32 driver already extends unit 0's 16-bit hardware
// counter past its native +-32767 range: it unconditionally arms high/low
// limit events at init and accumulates wraps in its own ISR, exposed via
// SENSOR_CHAN_ENCODER_COUNT. Reading it through the sensor API gets that
// extension for free; a second, competing interrupt registration on the
// unit does not fire.

namespace
{
    const struct device *g_dev = DEVICE_DT_GET(DT_NODELABEL(pcnt));

    constexpr int64_t STALE_MS = 100;

    K_MUTEX_DEFINE(g_filterMutex);
    int64_t g_filtered = 0;
    bool g_seeded = false;
    bool g_lastInvert = false;
    int32_t g_lastBacklash = 0;
    int64_t g_lastPollMs = 0;
    int64_t g_lastMoveMs = 0;
    Encoder::Direction g_lastDirection = Encoder::Direction::Stopped;
    atomic_t g_originCount = ATOMIC_INIT(0);
}

namespace Encoder
{
    void begin()
    {
        if (!device_is_ready(g_dev)) {
            LOG_ERR("pcnt device not ready");
        }
    }

    int64_t position()
    {
        struct sensor_value val = {0, 0};
        sensor_sample_fetch_chan(g_dev, SENSOR_CHAN_ENCODER_COUNT);
        sensor_channel_get(g_dev, SENSOR_CHAN_ENCODER_COUNT, &val);

        int64_t pos = val.val1;
        return Settings::g_active.invertEncoder ? -pos : pos;
    }

    int64_t origin()
    {
        return atomic_get(&g_originCount);
    }

    void setOrigin(int64_t count)
    {
        atomic_set(&g_originCount, (atomic_val_t)count);
    }

    int64_t filteredPosition()
    {
        int64_t raw = position();
        int64_t now = k_uptime_get();

        k_mutex_lock(&g_filterMutex, K_FOREVER);
        bool invert = Settings::g_active.invertEncoder;
        int32_t backlash = Settings::g_active.encoderBacklash;
        bool polled = now - g_lastPollMs <= STALE_MS;
        g_lastPollMs = now;
        int64_t previous = g_filtered;
        if (!g_seeded || invert != g_lastInvert || backlash != g_lastBacklash) {
            g_filtered = raw;
            g_seeded = true;
            g_lastInvert = invert;
            g_lastBacklash = backlash;
            previous = raw;
        } else if (raw > g_filtered + backlash) {
            g_filtered = raw - backlash;
        } else if (raw < g_filtered - backlash) {
            g_filtered = raw + backlash;
        }
        if (g_filtered != previous) {
            if (!polled) {
                g_lastDirection = Direction::Stopped;
            } else if (g_filtered > previous) {
                g_lastDirection = Direction::Forward;
            } else {
                g_lastDirection = Direction::Reverse;
            }
            g_lastMoveMs = now;
        }
        int64_t out = g_filtered;
        k_mutex_unlock(&g_filterMutex);

        return out;
    }

    Direction direction()
    {
        filteredPosition();
        k_mutex_lock(&g_filterMutex, K_FOREVER);
        Direction out = k_uptime_get() - g_lastMoveMs <= STALE_MS ? g_lastDirection : Direction::Stopped;
        k_mutex_unlock(&g_filterMutex);
        return out;
    }
}
