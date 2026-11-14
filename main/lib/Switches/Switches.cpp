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
#include "Switches.hpp"
#include "SwitchDebounce.hpp"
#include "Command.hpp"
#include "Events.hpp"
#include "Mode.hpp"
#include "Pins.hpp"
#include <string.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(switches, LOG_LEVEL_INF);

using PinMap::Fn;

static_assert((size_t)Fn::ModeProbe - (size_t)Fn::ModeGear + 1 == Mode::COUNT, "one mode switch per mode");

namespace
{
    constexpr int32_t POLL_PERIOD_MS = 5;
    constexpr uint8_t DEBOUNCE_SAMPLES = 4;
    constexpr size_t COMMAND_MAX = 32;

    struct Switch
    {
        gpio_dt_spec pin;
        const char *action;
        SwitchDebounce debounce;
    };

    Switch g_switches[PinMap::MAX_ASSIGNMENTS];
    size_t g_count = 0;
    char g_reply[Command::MAX_REPLY_LEN];

    K_THREAD_STACK_DEFINE(g_pollStack, 4096);
    struct k_thread g_pollThreadData;

    void run(const char *action)
    {
        while (*action) {
            size_t len = strcspn(action, ";");
            char line[COMMAND_MAX];
            if (len < sizeof(line)) {
                memcpy(line, action, len);
                line[len] = '\0';
                Command::apply(line, g_reply, sizeof(g_reply));
                if (strncmp(g_reply, "ok", 2) != 0) {
                    LOG_WRN("switch %s: %s", line, g_reply);
                    Events::publishRefusal(g_reply);
                }
            }
            action += len + (action[len] == ';');
        }
    }

    void pollLoop(void *, void *, void *)
    {
        while (true) {
            for (size_t i = 0; i < g_count; i++) {
                Switch &s = g_switches[i];
                if (s.debounce.pressed(gpio_pin_get_dt(&s.pin) > 0, DEBOUNCE_SAMPLES)) {
                    run(s.action);
                }
            }
            k_sleep(K_MSEC(POLL_PERIOD_MS));
        }
    }
}

namespace Switches
{
    void begin()
    {
        for (size_t f = 0; f < PinMap::FN_COUNT; f++) {
            const PinMap::FnDef &d = PinMap::def((Fn)f);
            if (!d.action) {
                continue;
            }
            for (size_t n = 0; n < Pins::count((Fn)f) && g_count < PinMap::MAX_ASSIGNMENTS; n++) {
                Switch &s = g_switches[g_count];
                if (!Pins::spec((Fn)f, n, 0, s.pin) || !gpio_is_ready_dt(&s.pin)) {
                    LOG_ERR("%s: GPIO not ready", d.id);
                    continue;
                }
                gpio_pin_configure_dt(&s.pin, GPIO_INPUT);
                s.action = d.action;
                // A switch held at boot does not fire.
                s.debounce = {gpio_pin_get_dt(&s.pin) > 0, 0};
                g_count++;
            }
        }
        if (!g_count) {
            return;
        }
        k_thread_create(&g_pollThreadData, g_pollStack, K_THREAD_STACK_SIZEOF(g_pollStack), pollLoop, nullptr,
                        nullptr, nullptr, K_PRIO_PREEMPT(7), 0, K_NO_WAIT);
    }
}
