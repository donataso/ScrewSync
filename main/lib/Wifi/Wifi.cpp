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
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <zephyr/kernel.h>
#include <zephyr/net/wifi_credentials.h>
#include <zephyr/net/dhcpv4.h>
#include <zephyr/net/dhcpv4_server.h>
#include <zephyr/net/hostname.h>
#include <zephyr/net/dns_sd.h>
#include "config.hpp"
#include "PersistBlob.hpp"
#include <zephyr/logging/log.h>

LOG_MODULE_DECLARE(main);

DNS_SD_REGISTER_TCP_SERVICE(WIFI_MDNS_HTTP, DEVICE_NAME, "_http", "local", "", 80);

namespace Wifi::Detail
{
    struct net_if *g_iface = nullptr;
    char g_ip[16] = {0};
    char g_curSsid[WIFI_SSID_MAX_LEN + 1] = {0};
    char g_lastSsid[WIFI_SSID_MAX_LEN + 1] = {0};
    bool g_enabled = true;
    bool g_apFallback = false;
    Mode g_mode = Mode::Off;
    bool g_triedLastSsid = false;
}

using namespace Wifi::Detail;

namespace
{
    const int64_t RETRY_BACKOFF_MS = 30000;
    const uint8_t QUICK_RETRY_MAX = 2;
    const int64_t QUICK_RETRY_MS = 1500;
    const int64_t CONNECT_TIMEOUT_S = 15;
    uint8_t g_quickRetries = 0;

    bool g_apUp = false;
    net_in_addr g_apAddr = {};
    char g_apName[32] = {0};
    char g_mdnsHost[32] = {0};

    int g_lastConnStatus = -1;
    int g_lastDisconnReason = -1;
    int64_t g_retryAt = 0;

    net_mgmt_event_callback g_wifiCb;

    // net_mgmt calls from the driver's event context deadlock it, so event handling runs on this work item.
    enum class PendingWifiWork : uint8_t
    {
        None,
        Connect,
        Disconnect,
        RestoreApIfIdle
    };
    PendingWifiWork g_pendingWifiWork = PendingWifiWork::None;
    wifi_status g_pendingStatus;
    struct k_work g_wifiWork;

    void radioOff()
    {
        net_mgmt(NET_REQUEST_WIFI_DISCONNECT, g_iface, NULL, 0);
        net_dhcpv4_stop(g_iface);
        stopAP();
        g_mode = Wifi::Mode::Off;
        g_ip[0] = '\0';
        g_curSsid[0] = '\0';
    }

    // AP only if fallback is on; otherwise Disconnected with a retry later.
    void enterNoConnect()
    {
        if (g_apFallback) {
            g_quickRetries = 0;
            startAP();
            return;
        }
        g_ip[0] = '\0';
        g_curSsid[0] = '\0';
        g_mode = Wifi::Mode::Disconnected;

        if (g_quickRetries < QUICK_RETRY_MAX) {
            g_quickRetries++;
            g_retryAt = k_uptime_get() + QUICK_RETRY_MS;
            LOG_INF("wifi: connect attempt failed -> retrying in %lld s (%u/%u)", (long long)(QUICK_RETRY_MS / 1000),
                    (unsigned)g_quickRetries, (unsigned)QUICK_RETRY_MAX);
            return;
        }

        g_retryAt = k_uptime_get() + RETRY_BACKOFF_MS;
        LOG_INF("wifi: no saved network reachable -> retry in %lld s (AP fallback off)",
                (long long)(RETRY_BACKOFF_MS / 1000));
    }

    void onConnectResult(const wifi_status *status)
    {
        g_lastConnStatus = status->conn_status;
        if (status->conn_status != WIFI_STATUS_CONN_SUCCESS) {
            LOG_WRN("wifi: connect failed (%s)", wifi_conn_status_txt((wifi_conn_status)status->conn_status));
            // The preferred SSID failed; any other saved network may still connect.
            if (g_triedLastSsid) {
                g_triedLastSsid = false;
                if (net_mgmt(NET_REQUEST_WIFI_CONNECT_STORED, g_iface, NULL, 0) == 0) {
                    return;
                }
            }
            enterNoConnect();
            return;
        }

        wifi_iface_status ifs = {};
        if (net_mgmt(NET_REQUEST_WIFI_IFACE_STATUS, g_iface, &ifs, sizeof(ifs)) == 0 && ifs.ssid_len) {
            size_t len = ifs.ssid_len < sizeof(g_curSsid) - 1 ? ifs.ssid_len : sizeof(g_curSsid) - 1;
            memcpy(g_curSsid, ifs.ssid, len);
            g_curSsid[len] = '\0';
        }

        stopAP();
        g_mode = Wifi::Mode::Station;
        g_quickRetries = 0;
        armWatchdog();
        net_dhcpv4_start(g_iface);
        if (g_curSsid[0]) {
            saveLastSsid(g_curSsid);
        }
        LOG_INF("wifi: connected to '%s'", g_curSsid);
    }

    void onDisconnectResult(const wifi_status *status)
    {
        g_lastDisconnReason = status->disconn_reason;
        LOG_INF("wifi: disconnected (%s)", wifi_disconn_reason_txt((wifi_disconn_reason)status->disconn_reason));
        net_dhcpv4_stop(g_iface);
        if (g_enabled) {
            enterNoConnect();
        }
    }

    void wifiWorkHandler(k_work *)
    {
        switch (g_pendingWifiWork) {
        case PendingWifiWork::Connect:
            onConnectResult(&g_pendingStatus);
            break;
        case PendingWifiWork::Disconnect:
            onDisconnectResult(&g_pendingStatus);
            break;
        case PendingWifiWork::RestoreApIfIdle:
            restoreApIfIdle();
            break;
        case PendingWifiWork::None:
            break;
        }
    }

    void submit(PendingWifiWork work)
    {
        g_pendingWifiWork = work;
        k_work_submit(&g_wifiWork);
    }

    void onWifiEvent(net_mgmt_event_callback *cb, uint64_t mgmtEvent, net_if *)
    {
        switch (mgmtEvent) {
        case NET_EVENT_WIFI_CONNECT_RESULT:
            g_pendingStatus = *(const wifi_status *)cb->info;
            submit(PendingWifiWork::Connect);
            break;
        case NET_EVENT_WIFI_DISCONNECT_RESULT:
            g_pendingStatus = *(const wifi_status *)cb->info;
            submit(PendingWifiWork::Disconnect);
            break;
        case NET_EVENT_WIFI_SCAN_RESULT:
            onScanResult((const wifi_scan_result *)cb->info);
            break;
        case NET_EVENT_WIFI_SCAN_DONE:
            onScanDone();
            submit(PendingWifiWork::RestoreApIfIdle);
            break;
        default:
            break;
        }
    }

    void persistFlag(const char *key, bool on)
    {
        uint8_t v = on ? 1 : 0;
        PersistBlob::save(NVS_NS, key, &v, 1);
    }

    bool loadFlag(const char *key, bool fallback)
    {
        uint8_t v = fallback ? 1 : 0;
        return PersistBlob::load(NVS_NS, key, &v, 1) ? (v != 0) : fallback;
    }

    void initNames()
    {
        struct net_linkaddr *mac = net_if_get_link_addr(g_iface);
        snprintf(g_apName, sizeof(g_apName), "%s-%02X%02X", DEVICE_NAME,
                 mac && mac->len >= 2 ? mac->addr[mac->len - 2] : 0, mac && mac->len >= 1 ? mac->addr[mac->len - 1] : 0);
        for (char *p = g_apName; *p; p++) {
            if (*p == ' ') {
                *p = '-';
            }
        }
        size_t i = 0;
        for (; g_apName[i] && i < sizeof(g_mdnsHost) - 1; i++) {
            g_mdnsHost[i] = (char)tolower((unsigned char)g_apName[i]);
        }
        g_mdnsHost[i] = '\0';
        net_hostname_set(g_mdnsHost, strlen(g_mdnsHost));
    }
}

namespace Wifi::Detail
{
    void startAP()
    {
        net_mgmt(NET_REQUEST_WIFI_DISCONNECT, g_iface, NULL, 0); // one radio: no station while an AP
        net_dhcpv4_stop(g_iface);

        static net_in_addr apNetmask;
        net_addr_pton(NET_AF_INET, "192.168.4.1", &g_apAddr);
        net_addr_pton(NET_AF_INET, "255.255.255.0", &apNetmask);
        net_if_ipv4_set_gw(g_iface, &g_apAddr);
        net_if_ipv4_addr_add(g_iface, &g_apAddr, NET_ADDR_MANUAL, 0);
        net_if_ipv4_set_netmask_by_addr(g_iface, &g_apAddr, &apNetmask);

        static net_in_addr dhcpPoolStart;
        dhcpPoolStart = g_apAddr;
        dhcpPoolStart.s4_addr[3] += 10;
        if (net_dhcpv4_server_start(g_iface, &dhcpPoolStart) != 0) {
            LOG_WRN("wifi: dhcpv4 server failed to start");
        }

        static wifi_connect_req_params apConfig;
        apConfig.ssid = (const uint8_t *)g_apName;
        apConfig.ssid_length = (uint8_t)strlen(g_apName);
        apConfig.security = WIFI_SECURITY_TYPE_NONE;
        apConfig.channel = WIFI_CHANNEL_ANY;
        apConfig.band = WIFI_FREQ_BAND_2_4_GHZ;

        if (net_mgmt(NET_REQUEST_WIFI_AP_ENABLE, g_iface, &apConfig, sizeof(apConfig)) != 0) {
            LOG_ERR("wifi: AP enable failed");
            net_dhcpv4_server_stop(g_iface);
            net_if_ipv4_addr_rm(g_iface, &g_apAddr);
            return;
        }

        g_apUp = true;
        strncpy(g_ip, "192.168.4.1", sizeof(g_ip) - 1);
        g_curSsid[0] = '\0';
        g_mode = Mode::AccessPoint;
        LOG_INF("wifi: AP '%s' up (192.168.4.1)", g_apName);
    }

    void stopAP()
    {
        if (!g_apUp) {
            return;
        }
        net_mgmt(NET_REQUEST_WIFI_AP_DISABLE, g_iface, NULL, 0);
        net_dhcpv4_server_stop(g_iface);
        net_if_ipv4_addr_rm(g_iface, &g_apAddr);
        g_apUp = false;
    }

    void restoreApIfIdle()
    {
        if (!g_apUp && g_enabled && wifi_credentials_is_empty() && g_mode != Mode::Connecting &&
            g_mode != Mode::Station) {
            startAP();
        }
    }

    bool connectSsid(const char *ssid, size_t ssidLen)
    {
        static wifi_credentials_personal creds;
        if (wifi_credentials_get_by_ssid_personal_struct(ssid, ssidLen, &creds) != 0) {
            return false;
        }

        static uint8_t ssidBuf[WIFI_SSID_MAX_LEN];
        static uint8_t pskBuf[WIFI_PSK_MAX_LEN];
        memcpy(ssidBuf, creds.header.ssid, creds.header.ssid_len);
        if (creds.password_len) {
            memcpy(pskBuf, creds.password, creds.password_len);
        }

        static wifi_connect_req_params params;
        params = {};
        params.ssid = ssidBuf;
        params.ssid_length = (uint8_t)creds.header.ssid_len;
        params.psk = pskBuf;
        params.psk_length = (uint8_t)creds.password_len;
        params.security = creds.password_len ? WIFI_SECURITY_TYPE_PSK : WIFI_SECURITY_TYPE_NONE;
        params.channel = WIFI_CHANNEL_ANY;
        params.band = WIFI_FREQ_BAND_UNKNOWN;
        params.timeout = CONNECT_TIMEOUT_S;

        stopAP();
        g_ip[0] = '\0';
        strncpy(g_curSsid, ssid, sizeof(g_curSsid) - 1);
        g_curSsid[sizeof(g_curSsid) - 1] = '\0';
        g_mode = Mode::Connecting;

        if (net_mgmt(NET_REQUEST_WIFI_CONNECT, g_iface, &params, sizeof(params)) != 0) {
            g_curSsid[0] = '\0';
            return false;
        }
        LOG_INF("wifi: connecting to '%s'", g_curSsid);
        return true;
    }

    // The last-known-good SSID first, then any other saved network.
    void beginConnectSequence()
    {
        if (!g_enabled) {
            radioOff();
            return;
        }
        if (wifi_credentials_is_empty()) {
            startAP();
            return;
        }

        g_mode = Mode::Connecting;
        g_curSsid[0] = '\0';

        if (g_lastSsid[0] && connectSsid(g_lastSsid, strlen(g_lastSsid))) {
            g_triedLastSsid = true;
            return;
        }

        g_triedLastSsid = false;
        stopAP();
        if (net_mgmt(NET_REQUEST_WIFI_CONNECT_STORED, g_iface, NULL, 0) != 0) {
            LOG_WRN("wifi: no saved network could be started");
            enterNoConnect();
        } else {
            LOG_INF("wifi: connecting to a saved network");
        }
    }
}

namespace Wifi
{
    void begin()
    {
        g_iface = net_if_get_wifi_sap();
        if (!g_iface) {
            LOG_ERR("wifi: no AP-capable interface found");
            return;
        }
        initNames();

        k_work_init(&g_wifiWork, wifiWorkHandler);
        net_mgmt_init_event_callback(&g_wifiCb, onWifiEvent,
                                     NET_EVENT_WIFI_CONNECT_RESULT | NET_EVENT_WIFI_DISCONNECT_RESULT |
                                         NET_EVENT_WIFI_SCAN_RESULT | NET_EVENT_WIFI_SCAN_DONE);
        net_mgmt_add_event_callback(&g_wifiCb);

        g_enabled = loadFlag("enabled", true);
        g_apFallback = loadFlag("apfb", false);
        loadLastSsid();

        beginConnectSequence();
    }

    void loop()
    {
        if (g_mode == Mode::Disconnected && !g_apFallback && k_uptime_get() >= g_retryAt) {
            beginConnectSequence();
        }
        if (g_mode == Mode::Station) {
            serviceWatchdog();
        }
        scanTick();
    }

    Mode mode()
    {
        return g_mode;
    }

    const char *modeName()
    {
        static const char *const NAMES[] = {"off", "connecting", "station", "ap", "disconnected"};
        return NAMES[(int)g_mode];
    }

    const char *ip()
    {
        if (g_mode != Mode::Station) {
            return g_ip;
        }
        net_in_addr *addr = net_if_ipv4_get_global_addr(g_iface, NET_ADDR_PREFERRED);
        if (!addr || !net_addr_ntop(NET_AF_INET, addr, g_ip, sizeof(g_ip))) {
            g_ip[0] = '\0';
        }
        return g_ip;
    }

    int rssi()
    {
        if (g_mode != Mode::Station) {
            return 0;
        }
        wifi_iface_status ifs = {};
        if (net_mgmt(NET_REQUEST_WIFI_IFACE_STATUS, g_iface, &ifs, sizeof(ifs)) != 0) {
            return 0;
        }
        return ifs.rssi;
    }

    const char *ssid()
    {
        return g_curSsid;
    }
    const char *apName()
    {
        return g_apName;
    }
    const char *mdnsHost()
    {
        return g_mdnsHost;
    }
    bool enabled()
    {
        return g_enabled;
    }
    bool apFallback()
    {
        return g_apFallback;
    }

    void setApFallback(bool on)
    {
        g_apFallback = on;
        persistFlag("apfb", on);
    }

    void apMode(bool on)
    {
        if (on) {
            startAP();
            return;
        }
        stopAP();
        beginConnectSequence();
    }

    const char *statusName()
    {
        return g_mode == Mode::Off ? "n/a" : wifi_conn_status_txt((wifi_conn_status)g_lastConnStatus);
    }

    const char *disconnectReason()
    {
        return g_lastDisconnReason < 0 ? "n/a" : wifi_disconn_reason_txt((wifi_disconn_reason)g_lastDisconnReason);
    }

    void setEnabled(bool on)
    {
        if (g_enabled == on) {
            return;
        }
        g_enabled = on;
        persistFlag("enabled", on);
        if (on) {
            beginConnectSequence();
        } else {
            radioOff();
        }
    }
}
