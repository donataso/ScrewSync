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
#include "WifiInternal.hpp"
#include <string.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

LOG_MODULE_DECLARE(main);

using namespace Wifi::Detail;

namespace
{
    const int64_t SCAN_TIMEOUT_MS = 15000;

    struct ScanAp
    {
        char ssid[WIFI_SSID_MAX_LEN + 1];
        int8_t rssi;
        bool locked;
    };
    ScanAp g_scanList[WIFI_SCAN_KEEP];
    uint8_t g_scanN = 0;
    bool g_scanBusy = false;
    int64_t g_scanDeadline = 0;

    bool listed(const uint8_t *ssid, size_t len)
    {
        for (uint8_t i = 0; i < g_scanN; i++) {
            if (strlen(g_scanList[i].ssid) == len && !strncmp(g_scanList[i].ssid, (const char *)ssid, len)) {
                return true;
            }
        }
        return false;
    }
}

namespace Wifi::Detail
{
    void onScanResult(const wifi_scan_result *entry)
    {
        LOG_INF("wifi ap=\"%.*s\" rssi=%d ch=%d enc=%d", (int)entry->ssid_length, entry->ssid, (int)entry->rssi,
                (int)entry->channel, (int)entry->security);

        if (g_scanN >= WIFI_SCAN_KEEP || entry->ssid_length == 0 || listed(entry->ssid, entry->ssid_length)) {
            return;
        }
        ScanAp &ap = g_scanList[g_scanN++];
        size_t len = MIN(entry->ssid_length, sizeof(ap.ssid) - 1);
        memcpy(ap.ssid, entry->ssid, len);
        ap.ssid[len] = '\0';
        ap.rssi = entry->rssi;
        ap.locked = entry->security != WIFI_SECURITY_TYPE_NONE;
    }

    void onScanDone()
    {
        g_scanBusy = false;
        LOG_INF("wifi scan done n=%u", (unsigned)g_scanN);
    }

    void scanTick()
    {
        if (g_scanBusy && k_uptime_get() >= g_scanDeadline) {
            g_scanBusy = false;
            LOG_WRN("wifi: scan timed out (no SCAN_DONE from the driver)");
            restoreApIfIdle();
        }
    }
}

namespace Wifi
{
    void scan()
    {
        if (g_scanBusy) {
            LOG_WRN("wifi: scan already in progress");
            return;
        }
        stopAP(); // one radio: no AP while scanning as a station
        g_scanN = 0;
        g_scanBusy = true;
        g_scanDeadline = k_uptime_get() + SCAN_TIMEOUT_MS;
        int ret = net_mgmt(NET_REQUEST_WIFI_SCAN, g_iface, NULL, 0);
        if (ret != 0) {
            g_scanBusy = false;
            LOG_WRN("wifi: scan request failed (%d)", ret);
            restoreApIfIdle();
        } else {
            LOG_INF("wifi: scan requested");
        }
    }

    bool scanBusy()
    {
        return g_scanBusy;
    }
    uint8_t scanCount()
    {
        return g_scanN;
    }
    const char *scanSsid(uint8_t i)
    {
        return i < g_scanN ? g_scanList[i].ssid : "";
    }
    int scanRssi(uint8_t i)
    {
        return i < g_scanN ? g_scanList[i].rssi : 0;
    }
    bool scanLocked(uint8_t i)
    {
        return i < g_scanN && g_scanList[i].locked;
    }
}
