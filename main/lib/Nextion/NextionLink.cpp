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
#include "NextionInternal.hpp"
#include <string.h>
#include <zephyr/drivers/uart.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/atomic.h>
#include <zephyr/sys/ring_buffer.h>

LOG_MODULE_REGISTER(nextion, LOG_LEVEL_INF);

namespace
{
    Nextion::Config g_config;
    const struct device *g_dev;

    constexpr size_t RX_RING_SIZE = 512;
    constexpr size_t FRAME_MAX = 128;
    constexpr uint32_t LINK_LOOP_MS = 20;
    constexpr size_t HEARTBEAT_EVERY_BYTES = 256;

    // Set while a transfer owns the UART; the link thread stays off it.
    atomic_t g_paused = ATOMIC_INIT(0);
    K_MUTEX_DEFINE(g_linkMutex);

    RING_BUF_DECLARE(g_rxRing, RX_RING_SIZE);
    struct k_spinlock g_rxLock;
    K_SEM_DEFINE(g_rxSem, 0, 1);

    K_THREAD_STACK_DEFINE(g_linkStack, 4096);
    struct k_thread g_linkThread;

    uint8_t g_frame[FRAME_MAX];
    size_t g_frameLen = 0;
    bool g_frameOverflow = false;
    int g_ffRun = 0;
    size_t g_bytesSinceHeartbeat = 0;

    void appendFrameByte(uint8_t b)
    {
        if (g_frameLen < sizeof(g_frame)) {
            g_frame[g_frameLen++] = b;
        } else {
            g_frameOverflow = true;
        }
    }

    void resetFrame()
    {
        g_frameLen = 0;
        g_frameOverflow = false;
        g_ffRun = 0;
    }

    // Frames end with 0xFF 0xFF 0xFF; fewer 0xFF are data.
    void feedByte(uint8_t b)
    {
        if (b == 0xFF)
        {
            if (++g_ffRun == 3) {
                if (g_frameOverflow) {
                    LOG_WRN("oversized frame dropped");
                } else {
                    g_config.onFrame(g_frame, g_frameLen);
                }
                resetFrame();
            }
            return;
        }
        for (; g_ffRun > 0; g_ffRun--) {
            appendFrameByte(0xFF);
        }
        appendFrameByte(b);
    }

    void uartIsr(const struct device *dev, void *)
    {
        uart_irq_update(dev);
        uint8_t buf[32];
        bool received = false;
        while (uart_irq_rx_ready(dev))
        {
            int n = uart_fifo_read(dev, buf, sizeof(buf));
            if (n <= 0) {
                break;
            }
            k_spinlock_key_t key = k_spin_lock(&g_rxLock);
            ring_buf_put(&g_rxRing, buf, (uint32_t)n);
            k_spin_unlock(&g_rxLock, key);
            received = true;
        }
        if (received) {
            k_sem_give(&g_rxSem);
        }
    }

    void drainRing()
    {
        uint8_t buf[64];
        uint32_t n;
        do {
            k_spinlock_key_t key = k_spin_lock(&g_rxLock);
            n = ring_buf_get(&g_rxRing, buf, sizeof(buf));
            k_spin_unlock(&g_rxLock, key);
            for (uint32_t i = 0; i < n; i++) {
                feedByte(buf[i]);
            }
        } while (n > 0);
    }

    void linkLoop(void *, void *, void *)
    {
        while (true)
        {
            k_sem_take(&g_rxSem, K_MSEC(LINK_LOOP_MS));
            k_mutex_lock(&g_linkMutex, K_FOREVER);
            if (!atomic_get(&g_paused))
            {
                drainRing();
                g_config.onService();
            }
            k_mutex_unlock(&g_linkMutex);
        }
    }

    // Caller holds g_linkMutex or the pause.
    void restartRx()
    {
        k_spinlock_key_t key = k_spin_lock(&g_rxLock);
        ring_buf_reset(&g_rxRing);
        k_spin_unlock(&g_rxLock, key);
        resetFrame();
        uart_irq_rx_enable(g_dev);
    }

    // Waits out a link pass already on the UART.
    void stopRx()
    {
        k_mutex_lock(&g_linkMutex, K_FOREVER);
        uart_irq_rx_disable(g_dev);
        k_mutex_unlock(&g_linkMutex);
    }
}

namespace Nextion::detail
{
    const Config &config() { return g_config; }

    void writeBytes(const uint8_t *data, size_t len)
    {
        for (size_t i = 0; i < len; i++) {
            uart_poll_out(g_dev, data[i]);
        }
    }

    void writeTerminator()
    {
        static const uint8_t END[] = {0xFF, 0xFF, 0xFF};
        writeBytes(END, sizeof(END));
    }

    void drainInput()
    {
        uint8_t b;
        while (uart_poll_in(g_dev, &b) == 0) {
        }
    }

    bool setBaud(uint32_t baud)
    {
        struct uart_config cfg = {
            .baudrate = baud,
            .parity = UART_CFG_PARITY_NONE,
            .stop_bits = UART_CFG_STOP_BITS_1,
            .data_bits = UART_CFG_DATA_BITS_8,
            .flow_ctrl = UART_CFG_FLOW_CTRL_NONE,
        };
        return uart_configure(g_dev, &cfg) == 0;
    }

    bool readByte(uint8_t &out, uint32_t timeoutMs)
    {
        uint32_t start = k_uptime_get_32();
        for (;;)
        {
            if (uart_poll_in(g_dev, &out) == 0) {
                return true;
            }
            if (k_uptime_get_32() - start >= timeoutMs) {
                return false;
            }
            k_sleep(K_MSEC(1));
        }
    }

    bool waitForByte(uint8_t expected, uint32_t timeoutMs)
    {
        uint32_t start = k_uptime_get_32();
        uint8_t b;
        while (k_uptime_get_32() - start < timeoutMs)
        {
            if (readByte(b, timeoutMs)) {
                if (b == expected) {
                    return true;
                }
                LOG_INF("unexpected byte 0x%02x while waiting for 0x%02x", b, expected);
            }
        }
        return false;
    }

    void seize()
    {
        atomic_set(&g_paused, 1);
        stopRx();
    }

    void release(bool restarted)
    {
        if (restarted) {
            resync();
        }
        restartRx();
        g_config.onResumed(restarted);
        atomic_set(&g_paused, 0);
    }
}

namespace Nextion
{
    void begin(const Config &config)
    {
        g_config = config;
        g_dev = config.uart;
        detail::setBaud(config.linkBaud);
        uart_irq_callback_user_data_set(g_dev, uartIsr, nullptr);
        restartRx();
    }

    void start()
    {
        k_thread_create(&g_linkThread, g_linkStack, K_THREAD_STACK_SIZEOF(g_linkStack), linkLoop, nullptr, nullptr,
                        nullptr, K_PRIO_PREEMPT(6), 0, K_NO_WAIT);
        k_thread_name_set(&g_linkThread, "display");
    }

    void writeRaw(const char *cmd)
    {
        detail::writeBytes((const uint8_t *)cmd, strlen(cmd));
        detail::writeTerminator();
    }

    void writeHeartbeat()
    {
        g_bytesSinceHeartbeat = 0;
        writeRaw("linkAge=0");
    }

    void writeCommand(const char *cmd)
    {
        writeRaw(cmd);
        g_bytesSinceHeartbeat += strlen(cmd) + 3;
        if (g_bytesSinceHeartbeat >= HEARTBEAT_EVERY_BYTES && !atomic_get(&g_paused)) {
            writeHeartbeat();
        }
    }

    bool pause()
    {
        if (!atomic_cas(&g_paused, 0, 1)) {
            return false;
        }
        stopRx();
        return true;
    }

    void release()
    {
        detail::release(false);
    }
}
