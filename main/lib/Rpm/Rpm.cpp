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
#include "Rpm.hpp"
#include "RpmCalc.hpp"
#include "Encoder.hpp"
#include "Settings.hpp"
#include "Machine.hpp"
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#ifdef CONFIG_SCREWSYNC_ENCODER_EDGE_LOG
#include "Pins.hpp"
#include <zephyr/drivers/gpio.h>
#endif

LOG_MODULE_REGISTER(rpm, LOG_LEVEL_INF);

// RPM is timed over one full encoder revolution, not a fixed window.
// A stall timeout reports 0 when pulses stop.

namespace
{
    constexpr int32_t POLL_PERIOD_MS = 20;
    constexpr int32_t STALL_TIMEOUT_MS = 500;
    constexpr int32_t ANOMALY_RPM_ABS = 2000;
    constexpr int64_t ANOMALY_LOG_MIN_INTERVAL_MS = 500;

    struct k_mutex g_mutex;
    int32_t g_currentRpm = 0;

    K_THREAD_STACK_DEFINE(g_pollStack, 2048);
    struct k_thread g_pollThreadData;

#ifdef CONFIG_SCREWSYNC_ENCODER_EDGE_LOG
    constexpr int64_t EDGE_LOG_PERIOD_MS = 1000;

    gpio_dt_spec g_pinA;
    gpio_dt_spec g_pinB;
    struct gpio_callback g_cbA;
    struct gpio_callback g_cbB;
    volatile uint8_t g_abState;
    atomic_t g_stepsUp;
    atomic_t g_stepsDown;
    atomic_t g_stepsSame;
    atomic_t g_stepsSkipped;

    uint8_t readAb()
    {
        return (uint8_t)((gpio_pin_get_raw(g_pinA.port, g_pinA.pin) << 1) | gpio_pin_get_raw(g_pinB.port, g_pinB.pin));
    }

    void onAbEdge(const struct device *, struct gpio_callback *, uint32_t)
    {
        static const int8_t STEP[16] = {0, 1, -1, 2, -1, 0, 2, 1, 1, 2, 0, -1, 2, -1, 1, 0};
        uint8_t now = readAb();
        int8_t step = STEP[(g_abState << 2) | now];
        g_abState = now;
        atomic_inc(step == 1 ? &g_stepsUp : step == -1 ? &g_stepsDown
                                        : step == 0    ? &g_stepsSame
                                                       : &g_stepsSkipped);
    }

    void beginEdgeLog()
    {
        if (!Pins::spec(PinMap::Fn::Spindle, 0, g_pinA) || !Pins::spec(PinMap::Fn::Spindle, 1, g_pinB)) {
            LOG_WRN("edge log: spindle pins not mapped");
            return;
        }
        LOG_INF("edge log: A=%s.%u B=%s.%u unit=%d filterNs=%u ppr=%d invert=%d", g_pinA.port->name, g_pinA.pin,
                g_pinB.port->name, g_pinB.pin, Pins::pcntUnit(PinMap::Fn::Spindle),
                (unsigned)Pins::filterNs(PinMap::Fn::Spindle), Settings::g_active.encoderPpr,
                Settings::g_active.invertEncoder);
        g_abState = readAb();
        gpio_init_callback(&g_cbA, onAbEdge, BIT(g_pinA.pin));
        gpio_init_callback(&g_cbB, onAbEdge, BIT(g_pinB.pin));
        gpio_add_callback(g_pinA.port, &g_cbA);
        gpio_add_callback(g_pinB.port, &g_cbB);
        gpio_pin_interrupt_configure_dt(&g_pinA, GPIO_INT_EDGE_BOTH);
        gpio_pin_interrupt_configure_dt(&g_pinB, GPIO_INT_EDGE_BOTH);
    }

    void logEdges(int64_t pos)
    {
        uint32_t up = (uint32_t)atomic_clear(&g_stepsUp);
        uint32_t down = (uint32_t)atomic_clear(&g_stepsDown);
        uint32_t same = (uint32_t)atomic_clear(&g_stepsSame);
        uint32_t skipped = (uint32_t)atomic_clear(&g_stepsSkipped);
        if (up || down || same || skipped) {
            LOG_INF("edges ab=%u%u up=%u down=%u same=%u skipped=%u count=%lld", g_abState >> 1, g_abState & 1, up,
                    down, same, skipped, pos);
        }
    }
#endif

    void pollLoop(void *, void *, void *)
    {
        int64_t markPos = Encoder::position();
        int64_t markUptimeMs = k_uptime_get();
        int64_t prevPollPos = markPos;
        int64_t lastAnomalyLogMs = 0;
#ifdef CONFIG_SCREWSYNC_ENCODER_EDGE_LOG
        int64_t lastEdgeLogMs = markUptimeMs;
        beginEdgeLog();
#endif

        while (true) {
            k_sleep(K_MSEC(POLL_PERIOD_MS));

            int64_t pos = Encoder::position();
            int64_t now = k_uptime_get();
            int64_t delta = pos - markPos;
            int32_t target = Settings::g_active.encoderPpr * 2;

            bool haveRpm = false;
            int32_t rpm = 0;
            if (target != 0 && (delta >= target || delta <= -target)) {
                rpm = Rpm::rpmFromDelta(delta, (uint32_t)(now - markUptimeMs), target);

                if ((rpm >= ANOMALY_RPM_ABS || rpm <= -ANOMALY_RPM_ABS) &&
                    now - lastAnomalyLogMs >= ANOMALY_LOG_MIN_INTERVAL_MS) {
                    LOG_WRN("anomaly rpm=%d delta=%lld elapsedMs=%lld pos=%lld markPos=%lld",
                            rpm, delta, now - markUptimeMs, pos, markPos);
                    lastAnomalyLogMs = now;
                }

                markPos = pos;
                markUptimeMs = now;
                haveRpm = true;
            } else if (pos == prevPollPos && (now - markUptimeMs) >= STALL_TIMEOUT_MS) {
                rpm = 0;
                markPos = pos;
                markUptimeMs = now;
                haveRpm = true;
            }
            prevPollPos = pos;
#ifdef CONFIG_SCREWSYNC_ENCODER_EDGE_LOG
            if (now - lastEdgeLogMs >= EDGE_LOG_PERIOD_MS) {
                logEdges(pos);
                lastEdgeLogMs = now;
            }
#endif

            if (haveRpm) {
                k_mutex_lock(&g_mutex, K_FOREVER);
                g_currentRpm = rpm;
                k_mutex_unlock(&g_mutex);
                Machine::setSpinning(rpm != 0);
            }
        }
    }
}

namespace Rpm
{
    void begin()
    {
        k_mutex_init(&g_mutex);
        k_thread_create(&g_pollThreadData, g_pollStack, K_THREAD_STACK_SIZEOF(g_pollStack),
                        pollLoop, nullptr, nullptr, nullptr,
                        K_PRIO_PREEMPT(5), 0, K_NO_WAIT);
    }

    int32_t value()
    {
        k_mutex_lock(&g_mutex, K_FOREVER);
        int32_t rpm = g_currentRpm;
        k_mutex_unlock(&g_mutex);
        return rpm;
    }
}
