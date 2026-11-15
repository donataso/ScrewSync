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
#include "HomeInput.hpp"
#include "Pins.hpp"
#include <zephyr/device.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/util.h>

LOG_MODULE_REGISTER(home_input, LOG_LEVEL_INF);

namespace
{
    using PinMap::Fn;
    using Stepper::Axis;

    constexpr size_t AXES = 2;

    struct Input
    {
        gpio_callback cb;
        gpio_dt_spec pin;
        Axis axis;
    };

    Input g_inputs[AXES] = {{{}, {}, Axis::Z}, {{}, {}, Axis::X}};
    HomeInput::Callback g_onEdge = nullptr;

    Input &inputOf(Axis axis)
    {
        return g_inputs[axis == Axis::Z ? 0 : 1];
    }

    bool read(const Input &in)
    {
        return in.pin.port && gpio_pin_get_dt(&in.pin) > 0;
    }

    void isr(const struct device *, struct gpio_callback *cb, uint32_t)
    {
        const Input &in = *CONTAINER_OF(cb, Input, cb);
        if (g_onEdge) {
            g_onEdge(in.axis, read(in));
        }
    }
}

namespace HomeInput
{
    void begin(Callback onEdge)
    {
        g_onEdge = onEdge;
        for (Input &in : g_inputs) {
            Fn fn = in.axis == Axis::Z ? Fn::ZHome : Fn::XHome;
            const char *name = in.axis == Axis::Z ? "Z" : "X";
            if (!Pins::spec(fn, 0, in.pin)) {
                continue;
            }
            if (!gpio_is_ready_dt(&in.pin)) {
                LOG_ERR("%s home GPIO not ready", name);
                in.pin = {};
                continue;
            }
            gpio_pin_configure_dt(&in.pin, GPIO_INPUT);
            gpio_pin_interrupt_configure_dt(&in.pin, GPIO_INT_EDGE_BOTH);
            gpio_init_callback(&in.cb, isr, BIT(in.pin.pin));
            gpio_add_callback(in.pin.port, &in.cb);
        }
        if (!sensors()) {
            LOG_INF("no home input");
        }
    }

    bool present(Axis axis)
    {
        return inputOf(axis).pin.port != nullptr;
    }

    bool active(Axis axis)
    {
        return read(inputOf(axis));
    }

    bool sensors()
    {
        return g_inputs[0].pin.port || g_inputs[1].pin.port;
    }
}
