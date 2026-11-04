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
#include "CommandInternal.hpp"
#include "Wifi.hpp"

namespace
{
    using Command::detail::skipSpaces;

    bool isIndex(const char *s) { return s[0] >= '0' && s[0] <= '9'; }

    void status(char *out, size_t outMax)
    {
        snprintf(out, outMax,
                 "ok mode=%s ip=%s rssi=%d ssid=%s ap=%s enabled=%d fallback=%d saved=%u "
                 "pref=\"%s\" last=%s reason=%s plen=%u",
                 Wifi::modeName(), Wifi::ip(), Wifi::rssi(), Wifi::ssid(), Wifi::apName(), (int)Wifi::enabled(),
                 (int)Wifi::apFallback(), (unsigned)Wifi::savedCount(), Wifi::preferredSsid(), Wifi::statusName(),
                 Wifi::disconnectReason(), Wifi::passLen());
    }

    void list(char *out, size_t outMax)
    {
        int n = snprintf(out, outMax, "ok n=%u pref=\"%s\"", (unsigned)Wifi::savedCount(), Wifi::preferredSsid());
        for (uint8_t i = 0; i < Wifi::savedCount() && n >= 0 && (size_t)n < outMax; i++)
        {
            n += snprintf(out + n, outMax - (size_t)n, " %u=\"%s\"%s", (unsigned)i, Wifi::savedSsid(i),
                          Wifi::savedIsPreferred(i) ? "(pref)" : "");
        }
    }

    // "add <name> [pass]" saves; "ssid <name> [pass]" saves and connects.
    void add(const char *a, bool connect, char *out, size_t outMax)
    {
        char ssid[33] = {0};
        char pass[65] = {0};
        sscanf(a, "%32s %64s", ssid, pass);
        if (!ssid[0]) {
            snprintf(out, outMax, "err wifi add <name> [pass]");
        } else if (!Wifi::addNetwork(ssid, pass)) {
            snprintf(out, outMax, "err add failed (list full or name too long)");
        } else {
            if (connect) {
                Wifi::useNetwork(ssid);
            }
            snprintf(out, outMax, "ok %s ssid=\"%s\"", connect ? "ssid" : "add", ssid);
        }
    }
}

namespace Command::detail
{
    void wifi(const char *args, char *out, size_t outMax)
    {
        const char *a = skipSpaces(args);
        if (!*a || !strcmp(a, "status")) {
            status(out, outMax);
        } else if (!strcmp(a, "list")) {
            list(out, outMax);
        } else if (!strcmp(a, "scan")) {
            Wifi::scan();
            snprintf(out, outMax, "ok scanning");
        } else if (!strcmp(a, "on") || !strcmp(a, "off")) {
            Wifi::setEnabled(a[1] == 'n');
            snprintf(out, outMax, "ok wifi %s", a);
        } else if (!strncmp(a, "ap ", 3)) {
            bool on = !strcmp(a + 3, "on");
            Wifi::apMode(on);
            snprintf(out, outMax, "ok ap %s", on ? "on" : "off");
        } else if (!strncmp(a, "fallback ", 9)) {
            Wifi::setApFallback(!strcmp(a + 9, "on"));
            snprintf(out, outMax, "ok fallback=%d", (int)Wifi::apFallback());
        } else if (!strcmp(a, "forget")) {
            snprintf(out, outMax, "ok wifi forget");
            Wifi::forget();
        } else if (!strncmp(a, "add ", 4)) {
            add(a + 4, false, out, outMax);
        } else if (!strncmp(a, "ssid ", 5)) {
            add(a + 5, true, out, outMax);
        } else if (!strncmp(a, "del ", 4)) {
            const char *t = skipSpaces(a + 4);
            bool ok = isIndex(t) ? Wifi::removeNetworkAt((uint8_t)atoi(t)) : Wifi::removeNetwork(t);
            snprintf(out, outMax, ok ? "ok del" : "err no such network");
        } else if (!strncmp(a, "use ", 4)) {
            const char *t = skipSpaces(a + 4);
            if (isIndex(t)) {
                Wifi::useNetworkAt((uint8_t)atoi(t));
            } else {
                Wifi::useNetwork(t);
            }
            snprintf(out, outMax, "ok use \"%s\"", t);
        } else {
            snprintf(out, outMax, "err wifi '%s'", a);
        }
    }
}
