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
#ifndef WifiInternal_h
#define WifiInternal_h

#include "Wifi.hpp"
#include <stddef.h>
#include <zephyr/net/net_if.h>
#include <zephyr/net/wifi_mgmt.h>

namespace Wifi::detail
{
    constexpr const char *NVS_NS = "wifi";

    extern struct net_if *g_iface;
    extern char g_ip[16];
    extern char g_curSsid[WIFI_SSID_MAX_LEN + 1];
    extern char g_lastSsid[WIFI_SSID_MAX_LEN + 1];
    extern bool g_enabled;
    extern bool g_apFallback;
    extern Mode g_mode;
    extern bool g_triedLastSsid;

    void startAP();
    void stopAP();
    // AP and STA share the one radio; brings the AP back once neither scan nor connect is in flight.
    void restoreApIfIdle();
    void beginConnectSequence();
    bool connectSsid(const char *ssid, size_t ssidLen);

    void loadLastSsid();
    void saveLastSsid(const char *ssid);

    void onScanResult(const wifi_scan_result *entry);
    void onScanDone();
    void scanTick();

    void armWatchdog();
    void serviceWatchdog();
}

#endif
