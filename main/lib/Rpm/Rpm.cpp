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

    void pollLoop(void *, void *, void *)
    {
        int64_t markPos = Encoder::position();
        int64_t markUptimeMs = k_uptime_get();
        int64_t prevPollPos = markPos;
        int64_t lastAnomalyLogMs = 0;

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
