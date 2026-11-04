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
#include "DisplayImage.hpp"
#include "RunSettings.hpp"
#include "Settings.hpp"
#include "WsCommand.hpp"

namespace Command::detail
{
    void settings(const char *args, char *out, size_t outMax)
    {
        const char *a = skipSpaces(args);
        if (!*a || !strcmp(a, "get"))
        {
            int n = snprintf(out, outMax, "ok ");
            Settings::formatAll(out + n, outMax - (size_t)n);
            return;
        }
        if (refuseUnlessIdle(out, outMax)) {
            return;
        }

        char reason[48];
        if (!strcmp(a, "reset"))
        {
            Settings::active = Settings::Conf{};
            Settings::save();
            snprintf(out, outMax, "ok reset");
        }
        else if (!strncmp(a, "set ", 4))
        {
            char name[32] = {0};
            char value[32] = {0};
            if (sscanf(a + 4, "%31s %31s", name, value) != 2) {
                snprintf(out, outMax, "err settings set <field> <value>");
            } else if (Settings::setByName(name, value, reason, sizeof(reason))) {
                snprintf(out, outMax, "ok %s=%s", name, value);
            } else {
                snprintf(out, outMax, "err %s", reason);
            }
        }
        else if (!strncmp(a, "set-all ", 8))
        {
            const char *js = skipSpaces(a + 8);
            if (Settings::setAllFromJson(js, strlen(js), reason, sizeof(reason))) {
                snprintf(out, outMax, "ok set-all");
            } else {
                snprintf(out, outMax, "err %s", reason);
            }
        }
        else
        {
            snprintf(out, outMax, "err settings '%s'", a);
        }
    }

    void display(const char *args, char *out, size_t outMax)
    {
        const char *a = skipSpaces(args);
        if (!*a || !strcmp(a, "info"))
        {
            DisplayImage::Info i = DisplayImage::info();
            if (i.stored) {
                snprintf(out, outMax, "ok stored=1 length=%u crc=%08x", (unsigned)i.length, (unsigned)i.crc);
            } else {
                snprintf(out, outMax, "ok stored=0");
            }
        }
        else if (!strcmp(a, "flash"))
        {
            if (refuseUnlessIdle(out, outMax)) {
                return;
            }
            const char *refusal = DisplayImage::flashStored();
            if (refusal) {
                snprintf(out, outMax, "err display flash: %s", refusal);
            } else {
                snprintf(out, outMax, "ok display flash started");
            }
        }
        else
        {
            snprintf(out, outMax, "err display info | flash");
        }
    }

    void units(const char *args, char *out, size_t outMax)
    {
        const char *a = skipSpaces(args);
        RunSettings::Units u;
        if (RunSettings::parseUnits(a, u)) {
            RunSettings::setUnits(u);
        } else if (*a && strcmp(a, "status")) {
            snprintf(out, outMax, "err units mm|inch");
            return;
        }
        snprintf(out, outMax, "ok units=%s", RunSettings::unitsName());
    }

    void ws(const char *args, char *out, size_t outMax)
    {
        const char *a = skipSpaces(args);
        if (!strcmp(a, "list"))
        {
            WsCommand::describeClients(out, outMax);
        }
        else if (!strncmp(a, "kill ", 5))
        {
            const char *refusal = WsCommand::kill(atoi(skipSpaces(a + 5)));
            if (refusal) {
                snprintf(out, outMax, "err ws kill: %s", refusal);
            } else {
                snprintf(out, outMax, "ok ws kill");
            }
        }
        else
        {
            snprintf(out, outMax, "err ws list | kill <n>");
        }
    }
}
