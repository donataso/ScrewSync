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
#include "Outputs.hpp"
#include "Events.hpp"
#include "Pins.hpp"
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(outputs, LOG_LEVEL_INF);

using PinMap::Fn;

namespace
{
    gpio_dt_spec g_running[PinMap::MAX_ASSIGNMENTS];
    size_t g_runningCount = 0;

    void onRunning(const struct zbus_channel *chan)
    {
        const auto *msg = static_cast<const Events::Running *>(zbus_chan_const_msg(chan));
        for (size_t i = 0; i < g_runningCount; i++) {
            gpio_pin_set_dt(&g_running[i], msg->running);
        }
    }
}

ZBUS_LISTENER_DEFINE(OUTPUTS_RUNNING_LIS, onRunning);
ZBUS_CHAN_ADD_OBS(RUNNING_CHAN, OUTPUTS_RUNNING_LIS, 3);

namespace Outputs
{
    void begin()
    {
        for (size_t n = 0; n < Pins::count(Fn::Running) && g_runningCount < PinMap::MAX_ASSIGNMENTS; n++) {
            gpio_dt_spec &pin = g_running[g_runningCount];
            if (!Pins::spec(Fn::Running, n, 0, pin) || !gpio_is_ready_dt(&pin)) {
                LOG_ERR("running: GPIO not ready");
                continue;
            }
            gpio_pin_configure_dt(&pin, GPIO_OUTPUT_INACTIVE);
            g_runningCount++;
        }
    }
}
