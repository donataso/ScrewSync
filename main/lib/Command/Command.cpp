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
#include "Command.hpp"
#include "CommandInternal.hpp"
#include "Machine.hpp"
#include "Motion.hpp"
#include <zephyr/sys/reboot.h>

using namespace Command::Detail;

namespace
{
    struct CommandDef
    {
        const char *token;
        const char *syntax;
        const char *description;
        Handler handler;
    };

    void ping(const char *, char *out, size_t outMax)
    {
        snprintf(out, outMax, "ok pong");
    }

    void reboot(const char *, char *out, size_t outMax)
    {
        snprintf(out, outMax, "ok reboot");
        sys_reboot(SYS_REBOOT_WARM);
    }

    void estop(const char *args, char *out, size_t outMax)
    {
        if (!strcmp(skipSpaces(args), "ack")) {
            snprintf(out, outMax, "ok estop ack, rebooting");
            Machine::ackEstop();
        } else {
            snprintf(out, outMax, "ok state=%s", Machine::stateName());
        }
    }

    void commands(const char *, char *out, size_t outMax);

    const CommandDef COMMANDS[] = {
        {"ping", "ping", "replies pong", ping},
        {"reboot", "reboot", "restarts the controller", reboot},
        {"estop", "estop | estop ack", "machine state query / clears a tripped estop and reboots", estop},
        {"wifi",
         "wifi status|list|scan|on|off|forget | add <name> [pass] | ssid <name> [pass] | "
         "del <name|idx> | use <name|idx> | ap on|off | fallback on|off",
         "WiFi AP+STA control", wifi},
        {"settings", "settings get | set <field> <value> | set-all <json> | reset", "machine config storage", settings},
        {"jog", "jog step [<deci-microns>] | z|x +|- [hold|release] | resolution <multiplier> | speed <steps/sec|0> | status",
         "jog buttons (tap = one step, repeated hold = continuous, release = stop), handwheel resolution/speed", jog},
        {"move", "move z|x to|by <deci-microns> | stop", "manual positioning", move},
        {"mode", "mode [<name>|status]", "selects the operating mode (refused while running)", mode},
        {"start", "start [zero]", "starts the selected mode; zero = rapid Z then X to 0 first", start},
        {"stop", "stop", "stops the mode and decelerates both axes", stop},
        {"skip", "skip", "running multi-pass mode: skips the next pass", skip},
        {"override", "override [status] | + | - | reset", "feed override of the running job, percent", override},
        {"gear", "gear [status] | pitch <du> | dir -|+ | axis z|x | stop-after <du>|off | taper <deg>",
         "gear mode (dir: feed toward -Z/-X or +Z/+X; taper: X follows, half-angle)", gear},
        {"async", "async [status] | speed <du/min> | dir -|+ | axis z|x | stop-after <du>|off", "async mode", async},
        {"turn",
         "turn [status] | pitch <du> | dir -|+ | axis z|x | length <du> | depth <du> | passes <n> | internal on|off",
         "turn mode (axis x = facing)", turn},
        {"thread",
         "thread [status] | preset <group> <designation> | pitch <du> | hand rh|lh | length <du> | "
         "depth <du> | passes <n> | starts <n> | internal on|off | taper <deg>",
         "thread mode", thread},
        {"threads", "threads groups | list <group> [<offset>] | reset", "thread table (edited over HTTP /threads)",
         threads},
        {"part", "part [status] | pitch <du> | depth <du> | peck <du>|off | internal on|off", "part mode", part},
        {"slot",
         "slot [status] | speed <du/min> | dir -|+ | length <du> | depth <du> | passes <n> | internal on|off",
         "slotting mode", slot},
        {"ellipse",
         "ellipse [status] | pitch <du> | dir -|+ | length <du> | depth <du> | passes <n> | concave on|off",
         "half-ellipse mode (dir picks the Z side)", ellipse},
        {"probe", "probe go z-|z+|x-|x+ | [status] | probe-dst <du> | offset <du>",
         "touch-off mode: starts a cycle on an axis, and its params/status", probe},
        {"display", "display info | flash", "stored display .tft: size and crc / flashes it to the display", display},
        {"units", "units [mm|inch]", "display units", units},
        {"ws", "ws list | kill <n>", "web UI WebSocket clients: numbered list / closes one", ws},
        {"zero", "zero z|x [<deci-microns>] | spindle",
         "sets the current position to 0 or the given value / current angle to 0 deg", zero},
        {"enable", "enable z|x on|off|status", "manual per-axis motor enable/disable switch", enable},
        {"commands", "commands", "list the command set", commands},
    };

    void commands(const char *, char *out, size_t outMax)
    {
        int n = snprintf(out, outMax, "ok");
        for (const CommandDef &c : COMMANDS) {
            if (n < 0 || (size_t)n >= outMax) {
                break;
            }
            n += snprintf(out + n, outMax - (size_t)n, "\n%s -- %s", c.syntax, c.description);
        }
    }
}

namespace Command::Detail
{
    bool parseAxis(const char *token, Stepper::Axis &axis)
    {
        if (!strcmp(token, "z")) {
            axis = Stepper::Axis::Z;
        } else if (!strcmp(token, "x")) {
            axis = Stepper::Axis::X;
        } else {
            return false;
        }
        return true;
    }

    bool takeAxis(const char *args, const char *verb, const char *usage, char (&axisTok)[4], Stepper::Axis &axis,
                  const char *&rest, char *out, size_t outMax)
    {
        const char *a = skipSpaces(args);
        int consumed = 0;
        axisTok[0] = '\0';
        if (sscanf(a, "%3s%n", axisTok, &consumed) != 1) {
            snprintf(out, outMax, "err %s", usage);
            return false;
        }
        if (!parseAxis(axisTok, axis)) {
            snprintf(out, outMax, "err %s axis must be z or x", verb);
            return false;
        }
        rest = skipSpaces(a + consumed);
        return true;
    }

    bool refuseUnlessIdle(char *out, size_t outMax)
    {
        if (Motion::idle()) {
            return false;
        }
        snprintf(out, outMax, "err machine not idle");
        return true;
    }
}

namespace Command
{
    void apply(const char *line, char *out, size_t outMax)
    {
        if (!line || !outMax) {
            return;
        }
        const char *a = skipSpaces(line);
        size_t tokLen = strcspn(a, " ");

        // "estop ack" is the only way out of a trip, on every transport.
        Machine::State st = Machine::state();
        bool isEstop = tokLen == 5 && !strncmp(a, "estop", 5);
        if (st == Machine::State::Flashing) {
            snprintf(out, outMax, "err busy: flashing");
            return;
        }
        if (st == Machine::State::EStop && !isEstop) {
            snprintf(out, outMax, "err busy: estop");
            return;
        }

        for (const CommandDef &c : COMMANDS) {
            if (strlen(c.token) == tokLen && !strncmp(a, c.token, tokLen)) {
                c.handler(a + tokLen, out, outMax);
                return;
            }
        }
        snprintf(out, outMax, "err unknown command '%.*s'", (int)tokLen, a);
    }
}
