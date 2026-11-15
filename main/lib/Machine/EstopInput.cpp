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
#include "EstopInput.hpp"
#include "Machine.hpp"
#include "Pins.hpp"
#include <zephyr/device.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(estop_input, LOG_LEVEL_INF);

namespace
{
    constexpr size_t MAX_INPUTS = 4;

    gpio_dt_spec g_pins[MAX_INPUTS] = {};
    gpio_callback g_cbs[MAX_INPUTS];
    size_t g_count = 0;

    void isr(const struct device *, struct gpio_callback *, uint32_t)
    {
        if (EstopInput::active()) {
            Machine::estopNow();
        }
    }
}

namespace EstopInput
{
    void begin()
    {
        size_t n = Pins::count(PinMap::Fn::Estop);
        if (n > MAX_INPUTS) {
            LOG_ERR("%u estop inputs, only the first %u are used", (unsigned)n, (unsigned)MAX_INPUTS);
            n = MAX_INPUTS;
        }
        for (size_t i = 0; i < n; i++) {
            gpio_dt_spec p;
            if (!Pins::spec(PinMap::Fn::Estop, i, 0, p) || !gpio_is_ready_dt(&p)) {
                LOG_ERR("estop input %u GPIO not ready", (unsigned)i);
                continue;
            }
            gpio_pin_configure_dt(&p, GPIO_INPUT);
            gpio_pin_interrupt_configure_dt(&p, GPIO_INT_EDGE_BOTH);
            gpio_init_callback(&g_cbs[g_count], isr, BIT(p.pin));
            gpio_add_callback(p.port, &g_cbs[g_count]);
            g_pins[g_count++] = p;
        }
    }

    bool active()
    {
        for (size_t i = 0; i < g_count; i++) {
            if (gpio_pin_get_dt(&g_pins[i]) > 0) {
                return true;
            }
        }
        return false;
    }
}
