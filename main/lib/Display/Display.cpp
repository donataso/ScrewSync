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
#include "DisplayInternal.hpp"
#include "Command.hpp"
#include "Machine.hpp"
#include "Mode.hpp"
#include "display_config.hpp"
#include <stdio.h>
#include <string.h>
#include <zephyr/device.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

#if !DT_HAS_CHOSEN(screwsync_display_uart)
#error "Nextion display needs chosen screwsync,display-uart in the target overlay"
#endif

LOG_MODULE_REGISTER(display, LOG_LEVEL_INF);

using namespace Display::Detail;
using namespace Display::Theme;
using namespace Nextion;

namespace
{
    constexpr size_t CMD_LINE_MAX = 128;

    uint32_t g_lastPingMs = 0;
    uint32_t g_lastPongMs = 0;
    uint32_t g_pageChangeMs = 0;
    bool g_alive = false;

    const char *describeMessage(const uint8_t *data, size_t len)
    {
        static char out[96];
        if (len >= 4 && data[0] == 0x65) {
            snprintf(out, sizeof(out), "touch: page %u, component %u, %s", data[1], data[2],
                     data[3] ? "press" : "release");
        } else if (len >= 2 && data[0] == 0x66) {
            snprintf(out, sizeof(out), "page now showing: %u", data[1]);
        } else if (len >= 6 && (data[0] == 0x67 || data[0] == 0x68)) {
            uint16_t x = (uint16_t)(data[1] << 8 | data[2]);
            uint16_t y = (uint16_t)(data[3] << 8 | data[4]);
            snprintf(out, sizeof(out), "touch coordinate: %u,%u, %s", x, y, data[5] ? "press" : "release");
        } else if (len >= 1 && data[0] == 0x86) {
            snprintf(out, sizeof(out), "device entered sleep");
        } else if (len >= 1 && data[0] == 0x87) {
            snprintf(out, sizeof(out), "device woke up");
        } else if (len >= 1 && data[0] == 0x88) {
            snprintf(out, sizeof(out), "device started up");
        } else if (len >= 1 && data[0] == 0x89) {
            snprintf(out, sizeof(out), "device starting SD card upgrade");
        } else if (len >= 1 && data[0] == 0x05) {
            snprintf(out, sizeof(out), "ack");
        } else {
            size_t hexLen = 0;
            for (size_t i = 0; i < len && hexLen + 3 < sizeof(out); i++) {
                hexLen += snprintf(out + hexLen, sizeof(out) - hexLen, "%02x ", data[i]);
            }
        }
        return out;
    }

    // "ui ..." lines are display-local navigation, not machine commands.
    void handleCommandLine(const uint8_t *data, size_t len)
    {
        char line[CMD_LINE_MAX];
        memcpy(line, data, len);
        line[len] = '\0';
        if (!strncmp(line, "ui threads ", 11)) {
            selectPickerGroup(line + 11);
            return;
        }
        if (!strncmp(line, "ui ", 3)) {
            return;
        }

        char reply[Command::MAX_REPLY_LEN];
        Command::apply(line, reply, sizeof(reply));
        if (!strncmp(reply, "err", 3)) {
            LOG_WRN("'%s' -> %s", line, reply);
            showToast(reply[3] == ' ' ? reply + 4 : reply);
        }
    }

    // The picker's attributes reset on a page change, page0's are global.
    void pageShown(uint8_t shown)
    {
        if (shown == page()) {
            return;
        }
        setPage(shown);
        forgetPage(PAGE_THREADS);
        if (shown == PAGE_THREADS) {
            openPicker();
        }
        pushSoon();
        g_pageChangeMs = g_lastPongMs;
    }

    // Globals of displays/nextion-5in/program.nx, outside the page attributes resync clears.
    void pushLink(bool heartbeat)
    {
        static int sentFlashing = -1;
        int flashing = Machine::state() == Machine::State::Flashing;
        if (heartbeat || flashing != sentFlashing) {
            sentFlashing = flashing;
            writeCommand(flashing ? "linkFlashing=1" : "linkFlashing=0");
        }
        if (heartbeat) {
            writeHeartbeat();
        }
    }

    // Losing the display stops motion: its buttons are the operator's stop.
    void linkLost()
    {
        g_alive = false;
        if (Mode::running() || Stepper::isMoving(Stepper::Axis::Z) || Stepper::isMoving(Stepper::Axis::X)) {
            LOG_WRN("display lost, stopping motion");
            Mode::stop();
        } else {
            LOG_WRN("display lost");
        }
    }

    void updateLiveness()
    {
        uint32_t now = k_uptime_get_32();
        bool heartbeat = now - g_lastPingMs >= DISPLAY_PING_INTERVAL_MS;
        if (heartbeat) {
            g_lastPingMs = now;
            writeCommand("sendme");
        }
        pushLink(heartbeat);
        if (g_alive && now - g_lastPongMs > DISPLAY_PING_TIMEOUT_MS &&
            now - g_pageChangeMs > DISPLAY_PAGE_LOAD_TIMEOUT_MS) {
            linkLost();
        }
    }

    void handleFrame(const uint8_t *data, size_t len)
    {
        if (len == 0) {
            return;
        }
        switch (data[0]) {
        case DISPLAY_CMD_LEAD_BYTE:
            handleCommandLine(data + 1, len - 1);
            break;
        case 0x66:
            g_lastPongMs = k_uptime_get_32();
            if (!g_alive) {
                g_alive = true;
                LOG_INF("display connected");
                resync();
            }
            if (len >= 2) {
                pageShown(data[1]);
            }
            break;
        case 0x88:
            LOG_INF("display started");
            clearPreview();
            writeCommand("bkcmd=2");
            setPage(PAGE_MAIN);
            resync();
            break;
        case 0x24:
            LOG_WRN("display serial buffer overflow");
            break;
        default:
            LOG_INF("rx: %s", describeMessage(data, len));
            break;
        }
    }

    void service()
    {
        updateLiveness();
        pushStatus();
    }

    void resumed(bool restarted)
    {
        g_lastPongMs = k_uptime_get_32();
        if (restarted) {
            g_alive = false;
        }
    }
}

namespace Display
{
    void begin()
    {
        Nextion::begin({DEVICE_DT_GET(DT_CHOSEN(screwsync_display_uart)), DISPLAY_LINK_BAUD, DISPLAY_UPLOAD_BAUD, handleFrame, service,
                        resumed});
        // No per-command return codes from the display.
        writeCommand("bkcmd=2");
        Nextion::start();
    }

    bool connected()
    {
        return g_alive;
    }
}
