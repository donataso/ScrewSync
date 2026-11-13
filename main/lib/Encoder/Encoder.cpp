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
#include "EncoderFilter.hpp"
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

    K_MUTEX_DEFINE(g_filterMutex);
    Encoder::Filter g_filter;
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
        int64_t out = g_filter.update(raw, now, Settings::g_active.invertEncoder, Settings::g_active.encoderBacklash);
        k_mutex_unlock(&g_filterMutex);

        return out;
    }

    Direction direction()
    {
        filteredPosition();
        k_mutex_lock(&g_filterMutex, K_FOREVER);
        Direction out = g_filter.direction(k_uptime_get());
        k_mutex_unlock(&g_filterMutex);
        return out;
    }
}
