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
    struct Lamp
    {
        Fn fn;
        gpio_dt_spec pins[PinMap::MAX_ASSIGNMENTS];
        size_t count;
        bool initial;
        bool on;
    };

    Lamp g_running = {Fn::RunningOut, {}, 0, false, false};
    Lamp g_estop = {Fn::EstopOut, {}, 0, true, true};

    void set(Lamp &lamp, bool on)
    {
        lamp.on = on;
        for (size_t i = 0; i < lamp.count; i++) {
            gpio_pin_set_dt(&lamp.pins[i], on);
        }
    }

    void onRunning(const struct zbus_channel *chan)
    {
        set(g_running, static_cast<const Events::Running *>(zbus_chan_const_msg(chan))->running);
    }

    void onEstop(const struct zbus_channel *chan)
    {
        set(g_estop, static_cast<const Events::Estop *>(zbus_chan_const_msg(chan))->estop);
    }

    void configure(Lamp &lamp, const char *name)
    {
        for (size_t n = 0; n < Pins::count(lamp.fn) && lamp.count < PinMap::MAX_ASSIGNMENTS; n++) {
            gpio_dt_spec &pin = lamp.pins[lamp.count];
            if (!Pins::spec(lamp.fn, n, 0, pin) || !gpio_is_ready_dt(&pin)) {
                LOG_ERR("%s: GPIO not ready", name);
                continue;
            }
            gpio_pin_configure_dt(&pin, lamp.initial ? GPIO_OUTPUT_ACTIVE : GPIO_OUTPUT_INACTIVE);
            lamp.count++;
        }
    }
}

ZBUS_LISTENER_DEFINE(OUTPUTS_RUNNING_LIS, onRunning);
ZBUS_CHAN_ADD_OBS(RUNNING_CHAN, OUTPUTS_RUNNING_LIS, 3);
ZBUS_LISTENER_DEFINE(OUTPUTS_ESTOP_LIS, onEstop);
ZBUS_CHAN_ADD_OBS(ESTOP_CHAN, OUTPUTS_ESTOP_LIS, 3);

namespace Outputs
{
    void begin()
    {
        configure(g_running, "running");
        configure(g_estop, "estop");
    }

    bool commanded(Fn fn)
    {
        return (fn == Fn::RunningOut ? g_running : g_estop).on;
    }
}
