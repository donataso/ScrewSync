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
#include "Handwheel.hpp"
#include "Pins.hpp"
#include "Settings.hpp"
#include <zephyr/device.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/atomic.h>
#include <hal/pcnt_ll.h>

LOG_MODULE_REGISTER(handwheel, LOG_LEVEL_INF);

// A handwheel is a PCNT unit (read on demand, plus a poll so the 16-bit
// counter cannot wrap unseen; this keeps the PCNT interrupt to the
// spindle), or a software decoder on GPIO interrupts.
// A PCNT unit counts the edges of A and B, so bounce on one line cancels,
// and is halved to match the software decoder, which counts A's edges.

namespace
{
    constexpr int32_t POLL_PERIOD_MS = 10;

    pcnt_dev_t *g_hw = PCNT_LL_GET_HW(0);

    struct Wheel
    {
        PinMap::Fn fn;
        int unit = -1;
        struct k_spinlock lock = {};
        int16_t last = 0;
        atomic_t count = ATOMIC_INIT(0);
        int shift = 0;
        gpio_dt_spec a = {};
        gpio_dt_spec b = {};
        gpio_callback cb = {};
    };

    Wheel g_z = {PinMap::Fn::ZHandwheel};
    Wheel g_x = {PinMap::Fn::XHandwheel};

    K_THREAD_STACK_DEFINE(g_pollStack, 1024);
    struct k_thread g_pollThreadData;

    void poll(Wheel &w)
    {
        if (w.unit < 0) {
            return;
        }
        k_spinlock_key_t key = k_spin_lock(&w.lock);
        int16_t cur = (int16_t)pcnt_ll_get_count(g_hw, w.unit);
        atomic_add(&w.count, (int16_t)(cur - w.last));
        w.last = cur;
        k_spin_unlock(&w.lock, key);
    }

    int64_t read(Wheel &w, bool invert)
    {
        poll(w);
        int64_t pos = (int64_t)atomic_get(&w.count) >> w.shift;
        return invert ? -pos : pos;
    }

    void pollLoop(void *, void *, void *)
    {
        while (true) {
            poll(g_z);
            poll(g_x);
            k_sleep(K_MSEC(POLL_PERIOD_MS));
        }
    }

    void onEdge(const struct device *, struct gpio_callback *cb, uint32_t)
    {
        Wheel &w = *CONTAINER_OF(cb, Wheel, cb);
        atomic_add(&w.count, gpio_pin_get_dt(&w.a) != gpio_pin_get_dt(&w.b) ? 1 : -1);
    }

    void setUp(Wheel &w)
    {
        switch (Pins::encoder(w.fn)) {
        case PinMap::Encoder::Hw:
            w.unit = Pins::pcntUnit(w.fn);
            w.shift = 1;
            if (w.unit >= 0) {
                w.last = (int16_t)pcnt_ll_get_count(g_hw, w.unit);
            }
            break;
        case PinMap::Encoder::Sw:
            if (!Pins::spec(w.fn, 0, w.a) || !Pins::spec(w.fn, 1, w.b) || !gpio_is_ready_dt(&w.a) ||
                !gpio_is_ready_dt(&w.b)) {
                LOG_ERR("%s: pins not ready", PinMap::def(w.fn).id);
                return;
            }
            gpio_pin_configure_dt(&w.a, GPIO_INPUT);
            gpio_pin_configure_dt(&w.b, GPIO_INPUT);
            gpio_init_callback(&w.cb, onEdge, BIT(w.a.pin));
            gpio_add_callback(w.a.port, &w.cb);
            gpio_pin_interrupt_configure_dt(&w.a, GPIO_INT_EDGE_BOTH);
            break;
        default:
            break;
        }
    }
}

namespace Handwheel
{
    void begin()
    {
        setUp(g_z);
        setUp(g_x);
        if (g_z.unit >= 0 || g_x.unit >= 0) {
            k_thread_create(&g_pollThreadData, g_pollStack, K_THREAD_STACK_SIZEOF(g_pollStack), pollLoop, nullptr,
                            nullptr, nullptr, K_PRIO_PREEMPT(5), 0, K_NO_WAIT);
        }
    }

    int64_t z()
    {
        return read(g_z, Settings::g_active.z.invertHandwheel);
    }

    int64_t x()
    {
        return read(g_x, Settings::g_active.x.invertHandwheel);
    }
}
