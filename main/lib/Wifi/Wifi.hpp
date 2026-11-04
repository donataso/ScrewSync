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
#ifndef Wifi_h
#define Wifi_h

#include <stdint.h>

#ifndef WIFI_MAX_APS
#define WIFI_MAX_APS 5
#endif

#ifndef WIFI_SCAN_KEEP
#define WIFI_SCAN_KEEP 24 // APs cached from the last scan for the web UI
#endif

namespace Wifi
{
    enum class Mode : uint8_t
    {
        Off,
        Connecting,
        Station,
        AccessPoint,
        Disconnected, // saved networks exist, none reachable, AP fallback off
    };

    void begin();
    void loop();

    Mode mode();
    // "off"|"connecting"|"station"|"ap"|"disconnected"
    const char *modeName();
    const char *ip();
    int rssi();
    const char *ssid();
    const char *apName();
    const char *mdnsHost();
    bool enabled();

    uint8_t savedCount();
    const char *savedSsid(uint8_t i);
    bool savedIsPreferred(uint8_t i);
    const char *preferredSsid();
    unsigned passLen();

    bool addNetwork(const char *ssid, const char *pass);
    bool removeNetwork(const char *ssid);
    bool removeNetworkAt(uint8_t i);
    void useNetworkAt(uint8_t i);
    void useNetwork(const char *ssid);

    bool apFallback();
    void setApFallback(bool on);
    void apMode(bool on);

    const char *statusName();
    const char *disconnectReason();
    void scan();

    bool scanBusy();
    uint8_t scanCount();
    const char *scanSsid(uint8_t i);
    int scanRssi(uint8_t i);
    bool scanLocked(uint8_t i);

    void saveCredentials(const char *ssid, const char *pass);
    void setCredentials(const char *ssid, const char *pass);
    void setEnabled(bool on);
    void forget();
}

#endif
