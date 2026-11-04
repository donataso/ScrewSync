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
#include <zephyr/device.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(probe_input, LOG_LEVEL_INF);

namespace
{
    const gpio_dt_spec g_probe = GPIO_DT_SPEC_GET(DT_PATH(zephyr_user), probe_gpios);
    gpio_callback g_cb;
    ProbeInput::Callback g_onEdge = nullptr;

    bool rawLevel() { return gpio_pin_get_dt(&g_probe) > 0; }

    void isr(const struct device *, struct gpio_callback *, uint32_t)
    {
        if (g_onEdge) {
            g_onEdge(ProbeInput::fromRaw(rawLevel(), Settings::active.probePolarityNc));
        }
    }
}

namespace ProbeInput
{
    void begin(Callback onEdge)
    {
        g_onEdge = onEdge;
        if (!gpio_is_ready_dt(&g_probe)) {
            LOG_ERR("probe GPIO not ready");
            return;
        }
        gpio_pin_configure_dt(&g_probe, GPIO_INPUT);
        gpio_pin_interrupt_configure_dt(&g_probe, GPIO_INT_EDGE_BOTH);
        gpio_init_callback(&g_cb, isr, BIT(g_probe.pin));
        gpio_add_callback(g_probe.port, &g_cb);
    }

    bool contact()
    {
        return fromRaw(rawLevel(), Settings::active.probePolarityNc);
    }
}
