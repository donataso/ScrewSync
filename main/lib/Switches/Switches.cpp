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
#include "JogButton.hpp"
#include "JogStepDial.hpp"
#include "Command.hpp"
#include "Events.hpp"
#include "Mode.hpp"
#include "Pins.hpp"
#include "display_config.hpp"
#include <stdio.h>
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
    // The same timing as the display's jog buttons.
    constexpr uint32_t JOG_HOLD_DELAY_MS = 3 * JOG_REPEAT_INTERVAL_MS;

    struct JogFn
    {
        Fn fn;
        const char *axisDir;
    };

    const JogFn JOG_FNS[] = {
        {Fn::JogZPlus, "z +"},
        {Fn::JogZMinus, "z -"},
        {Fn::JogXPlus, "x +"},
        {Fn::JogXMinus, "x -"},
    };

    struct Switch
    {
        gpio_dt_spec pin;
        const char *action;
        // Jog button: "<axis> <dir>", else nullptr.
        const char *jog;
        SwitchDebounce debounce;
        JogButton button;
    };

    Switch g_switches[PinMap::MAX_ASSIGNMENTS];
    size_t g_count = 0;
    char g_reply[Command::MAX_REPLY_LEN];
    bool g_dial = false;

    K_THREAD_STACK_DEFINE(g_pollStack, 4096);
    struct k_thread g_pollThreadData;

    // False if the command was refused.
    bool apply(const char *line)
    {
        Command::apply(line, g_reply, sizeof(g_reply));
        if (strncmp(g_reply, "ok", 2) == 0) {
            return true;
        }
        LOG_WRN("switch %s: %s", line, g_reply);
        Events::publishRefusal(g_reply);
        return false;
    }

    void run(const char *action)
    {
        while (*action) {
            size_t len = strcspn(action, ";");
            char line[COMMAND_MAX];
            if (len < sizeof(line)) {
                memcpy(line, action, len);
                line[len] = '\0';
                apply(line);
            }
            action += len + (action[len] == ';');
        }
    }

    void jog(Switch &s, uint32_t nowMs)
    {
        char line[COMMAND_MAX];
        switch (s.button.update(s.debounce.stable, nowMs, JOG_HOLD_DELAY_MS, JOG_REPEAT_INTERVAL_MS)) {
        case JogButton::Event::Tap:
            snprintf(line, sizeof(line), "jog %s", s.jog);
            apply(line);
            break;
        case JogButton::Event::Hold:
            snprintf(line, sizeof(line), "jog %s hold", s.jog);
            if (!apply(line)) {
                s.button.blocked = true;
            }
            break;
        case JogButton::Event::Release:
            snprintf(line, sizeof(line), "jog %s release", s.jog);
            apply(line);
            break;
        case JogButton::Event::None:
            break;
        }
    }

    void pollLoop(void *, void *, void *)
    {
        while (true) {
            uint32_t nowMs = k_uptime_get_32();
            for (size_t i = 0; i < g_count; i++) {
                Switch &s = g_switches[i];
                bool pressed = s.debounce.pressed(gpio_pin_get_dt(&s.pin) > 0, DEBOUNCE_SAMPLES);
                if (s.jog) {
                    jog(s, nowMs);
                } else if (pressed) {
                    run(s.action);
                }
            }
            if (g_dial) {
                JogStepDial::tick(nowMs);
            }
            k_sleep(K_MSEC(POLL_PERIOD_MS));
        }
    }

    const char *jogOf(Fn fn)
    {
        for (const JogFn &j : JOG_FNS) {
            if (j.fn == fn) {
                return j.axisDir;
            }
        }
        return nullptr;
    }
}

namespace Switches
{
    void begin()
    {
        for (size_t f = 0; f < PinMap::FN_COUNT; f++) {
            const PinMap::FnDef &d = PinMap::def((Fn)f);
            const char *jog = jogOf((Fn)f);
            if (!d.action && !jog) {
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
                s.jog = jog;
                // A switch held at boot does not fire.
                bool held = gpio_pin_get_dt(&s.pin) > 0;
                s.debounce = {held, 0};
                s.button = {false, false, held, 0, 0};
                g_count++;
            }
        }
        g_dial = Pins::count(Fn::JogStepEncoder) > 0;
        if (!g_count && !g_dial) {
            return;
        }
        k_thread_create(&g_pollThreadData, g_pollStack, K_THREAD_STACK_SIZEOF(g_pollStack), pollLoop, nullptr,
                        nullptr, nullptr, K_PRIO_PREEMPT(7), 0, K_NO_WAIT);
    }
}
