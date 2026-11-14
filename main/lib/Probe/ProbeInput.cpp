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
#include "ProbeInput.hpp"
#include "Settings.hpp"
#include "Pins.hpp"
#include <zephyr/device.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(probe_input, LOG_LEVEL_INF);

namespace
{
    constexpr size_t MAX_PROBES = 4;

    gpio_dt_spec g_probes[MAX_PROBES] = {};
    gpio_callback g_cbs[MAX_PROBES];
    size_t g_count = 0;
    ProbeInput::Callback g_onEdge = nullptr;

    bool anyContact()
    {
        bool nc = Settings::g_active.probePolarityNc;
        for (size_t i = 0; i < g_count; i++) {
            if (ProbeInput::fromRaw(gpio_pin_get_dt(&g_probes[i]) > 0, nc)) {
                return true;
            }
        }
        return false;
    }

    void isr(const struct device *, struct gpio_callback *, uint32_t)
    {
        if (g_onEdge) {
            g_onEdge(anyContact());
        }
    }
}

namespace ProbeInput
{
    void begin(Callback onEdge)
    {
        g_onEdge = onEdge;
        size_t n = Pins::count(PinMap::Fn::Probe);
        if (n > MAX_PROBES) {
            LOG_ERR("%u probes, only the first %u are used", (unsigned)n, (unsigned)MAX_PROBES);
            n = MAX_PROBES;
        }
        for (size_t i = 0; i < n; i++) {
            gpio_dt_spec p;
            if (!Pins::spec(PinMap::Fn::Probe, i, 0, p) || !gpio_is_ready_dt(&p)) {
                LOG_ERR("probe %u GPIO not ready", (unsigned)i);
                continue;
            }
            gpio_pin_configure_dt(&p, GPIO_INPUT);
            gpio_pin_interrupt_configure_dt(&p, GPIO_INT_EDGE_BOTH);
            gpio_init_callback(&g_cbs[g_count], isr, BIT(p.pin));
            gpio_add_callback(p.port, &g_cbs[g_count]);
            g_probes[g_count++] = p;
        }
        if (!g_count) {
            LOG_INF("probe has no pin");
        }
    }

    bool contact()
    {
        return anyContact();
    }
}
