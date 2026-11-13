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
#include "Display.hpp"
#ifdef CONFIG_SCREWSYNC_DISPLAY_IMAGE
#include "DisplayImage.hpp"
#endif
#include "Ota.hpp"
#include <zephyr/drivers/hwinfo.h>
#include <zephyr/kernel.h>

#ifndef FW_VERSION
#error "FW_VERSION is not set"
#endif
#ifndef SCREWSYNC_TARGET
#error "SCREWSYNC_TARGET is not set"
#endif
#include "RunSettings.hpp"
#include "Settings.hpp"
#include "WsCommand.hpp"

namespace Command::Detail
{
    void settings(const char *args, char *out, size_t outMax)
    {
        const char *a = skipSpaces(args);
        if (!*a || !strcmp(a, "get")) {
            int n = snprintf(out, outMax, "ok ");
            Settings::formatAll(out + n, outMax - (size_t)n);
            return;
        }
        if (refuseUnlessIdle(out, outMax)) {
            return;
        }

        char reason[48];
        if (!strcmp(a, "reset")) {
            Settings::g_active = Settings::Conf{};
            Settings::save();
            snprintf(out, outMax, "ok reset");
        } else if (!strncmp(a, "set ", 4)) {
            char name[32] = {0};
            char value[32] = {0};
            if (sscanf(a + 4, "%31s %31s", name, value) != 2) {
                snprintf(out, outMax, "err settings set <field> <value>");
            } else if (Settings::setByName(name, value, reason, sizeof(reason))) {
                snprintf(out, outMax, "ok %s=%s", name, value);
            } else {
                snprintf(out, outMax, "err %s", reason);
            }
        } else if (!strncmp(a, "set-all ", 8)) {
            const char *js = skipSpaces(a + 8);
            if (Settings::setAllFromJson(js, strlen(js), reason, sizeof(reason))) {
                snprintf(out, outMax, "ok set-all");
            } else {
                snprintf(out, outMax, "err %s", reason);
            }
        } else {
            snprintf(out, outMax, "err settings '%s'", a);
        }
    }

    void info(const char *, char *out, size_t outMax)
    {
        static constexpr struct
        {
            uint32_t bit;
            const char *name;
        } CAUSES[] = {
            {RESET_PIN, "pin"},
            {RESET_SOFTWARE, "software"},
            {RESET_BROWNOUT, "brownout"},
            {RESET_POR, "power-on"},
            {RESET_WATCHDOG, "watchdog"},
            {RESET_DEBUG, "debug"},
            {RESET_SECURITY, "security"},
            {RESET_LOW_POWER_WAKE, "wake"},
            {RESET_CPU_LOCKUP, "lockup"},
            {RESET_PARITY, "parity"},
            {RESET_PLL, "pll"},
            {RESET_CLOCK, "clock"},
            {RESET_HARDWARE, "hardware"},
            {RESET_USER, "user"},
            {RESET_TEMPERATURE, "temperature"},
        };
        uint32_t cause = 0;
        char causes[96] = "unknown";
        if (hwinfo_get_reset_cause(&cause) == 0) {
            size_t n = 0;
            causes[0] = '\0';
            for (const auto &c : CAUSES) {
                if ((cause & c.bit) && n < sizeof(causes)) {
                    n += (size_t)snprintf(causes + n, sizeof(causes) - n, "%s%s", n ? "," : "", c.name);
                }
            }
            if (!n) {
                snprintf(causes, sizeof(causes), "none");
            }
        }
        snprintf(out, outMax, "ok version=%s target=%s reset=%s uptime=%lld confirmed=%d", FW_VERSION, SCREWSYNC_TARGET,
                 causes, k_uptime_get() / 1000, (int)Ota::isConfirmed());
    }

    void display(const char *args, char *out, size_t outMax)
    {
        const char *a = skipSpaces(args);
        if (!strcmp(a, "status")) {
            snprintf(out, outMax, "ok connected=%d", (int)Display::connected());
            return;
        }
#ifdef CONFIG_SCREWSYNC_DISPLAY_IMAGE
        if (!*a || !strcmp(a, "info")) {
            DisplayImage::Info i = DisplayImage::info();
            if (i.stored) {
                snprintf(out, outMax, "ok stored=1 length=%u crc=%08x", (unsigned)i.length, (unsigned)i.crc);
            } else {
                snprintf(out, outMax, "ok stored=0");
            }
        } else if (!strcmp(a, "flash")) {
            if (refuseUnlessIdle(out, outMax)) {
                return;
            }
            const char *refusal = DisplayImage::flashStored();
            if (refusal) {
                snprintf(out, outMax, "err display flash: %s", refusal);
            } else {
                snprintf(out, outMax, "ok display flash started");
            }
        } else {
            snprintf(out, outMax, "err display status | info | flash");
        }
#else
        snprintf(out, outMax, "err display status");
#endif
    }

    void units(const char *args, char *out, size_t outMax)
    {
        const char *a = skipSpaces(args);
        RunSettings::Units u;
        if (RunSettings::parseUnits(a, u)) {
            RunSettings::setUnits(u);
        } else if (*a && strcmp(a, "status") != 0) {
            snprintf(out, outMax, "err units mm|inch");
            return;
        }
        snprintf(out, outMax, "ok units=%s", RunSettings::unitsName());
    }

    void ws(const char *args, char *out, size_t outMax)
    {
        const char *a = skipSpaces(args);
        if (!strcmp(a, "list")) {
            WsCommand::describeClients(out, outMax);
        } else if (!strncmp(a, "kill ", 5)) {
            const char *refusal = WsCommand::kill(atoi(skipSpaces(a + 5)));
            if (refusal) {
                snprintf(out, outMax, "err ws kill: %s", refusal);
            } else {
                snprintf(out, outMax, "ok ws kill");
            }
        } else {
            snprintf(out, outMax, "err ws list | kill <n>");
        }
    }
}
