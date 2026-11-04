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
#include <zephyr/net/wifi_credentials.h>
#include <zephyr/sys/reboot.h>
#include "PersistBlob.hpp"

using namespace Wifi::detail;

namespace
{
    struct SavedCache
    {
        char ssid[WIFI_MAX_APS][WIFI_SSID_MAX_LEN + 1];
        uint8_t n;
    };

    void collectSsid(void *arg, const char *ssid, size_t ssidLen)
    {
        SavedCache *c = (SavedCache *)arg;
        if (c->n >= WIFI_MAX_APS) {
            return;
        }
        size_t len = ssidLen < sizeof(c->ssid[0]) - 1 ? ssidLen : sizeof(c->ssid[0]) - 1;
        memcpy(c->ssid[c->n], ssid, len);
        c->ssid[c->n][len] = '\0';
        c->n++;
    }

    SavedCache savedCache()
    {
        SavedCache c{};
        wifi_credentials_for_each_ssid(collectSsid, &c);
        return c;
    }
}

namespace Wifi::detail
{
    void loadLastSsid()
    {
        size_t n = PersistBlob::load(NVS_NS, "last", g_lastSsid, sizeof(g_lastSsid) - 1);
        g_lastSsid[n] = '\0';
    }

    void saveLastSsid(const char *ssid)
    {
        if (!strcmp(g_lastSsid, ssid)) {
            return;
        }
        strncpy(g_lastSsid, ssid, sizeof(g_lastSsid) - 1);
        g_lastSsid[sizeof(g_lastSsid) - 1] = '\0';
        PersistBlob::save(NVS_NS, "last", g_lastSsid, strlen(g_lastSsid) + 1);
    }
}

namespace Wifi
{
    uint8_t savedCount() { return savedCache().n; }

    const char *savedSsid(uint8_t i)
    {
        static SavedCache c;
        c = savedCache();
        return i < c.n ? c.ssid[i] : "";
    }

    bool savedIsPreferred(uint8_t i)
    {
        SavedCache c = savedCache();
        return i < c.n && !strcmp(c.ssid[i], g_lastSsid);
    }

    const char *preferredSsid() { return g_lastSsid; }

    unsigned passLen()
    {
        if (!g_curSsid[0]) {
            return 0;
        }
        wifi_credentials_personal creds;
        if (wifi_credentials_get_by_ssid_personal_struct(g_curSsid, strlen(g_curSsid), &creds) != 0) {
            return 0;
        }
        return (unsigned)creds.password_len;
    }

    bool addNetwork(const char *ssid, const char *pass)
    {
        if (!ssid || !ssid[0]) {
            return false;
        }
        size_t plen = pass ? strlen(pass) : 0;
        wifi_security_type sec = plen ? WIFI_SECURITY_TYPE_PSK : WIFI_SECURITY_TYPE_NONE;
        return wifi_credentials_set_personal(ssid, strlen(ssid), sec, NULL, 0, pass, plen, 0, 0, 0) == 0;
    }

    bool removeNetwork(const char *ssid)
    {
        if (!ssid || !ssid[0]) {
            return false;
        }
        bool ok = wifi_credentials_delete_by_ssid(ssid, strlen(ssid)) == 0;
        if (ok && !strcmp(g_lastSsid, ssid)) {
            saveLastSsid("");
        }
        return ok;
    }

    bool removeNetworkAt(uint8_t i)
    {
        SavedCache c = savedCache();
        return i < c.n && removeNetwork(c.ssid[i]);
    }

    void useNetworkAt(uint8_t i)
    {
        SavedCache c = savedCache();
        if (i < c.n) {
            useNetwork(c.ssid[i]);
        }
    }

    void useNetwork(const char *ssid)
    {
        if (!ssid || !ssid[0]) {
            return;
        }
        saveLastSsid(ssid);
        g_triedLastSsid = false; // picked explicitly: no fallback to another network on failure
        connectSsid(ssid, strlen(ssid));
    }

    void saveCredentials(const char *ssid, const char *pass) { addNetwork(ssid, pass); }

    void setCredentials(const char *ssid, const char *pass)
    {
        addNetwork(ssid, pass);
        useNetwork(ssid);
    }

    void forget()
    {
        wifi_credentials_delete_all();
        saveLastSsid("");
        sys_reboot(SYS_REBOOT_WARM);
    }
}
