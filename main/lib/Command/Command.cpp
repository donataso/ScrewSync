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
#include "GcodeMdi.hpp"
#include "Machine.hpp"
#include "Mode.hpp"
#include "Motion.hpp"
#include "Pins.hpp"
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
        const char *arg = skipSpaces(args);
        if (!strcmp(arg, "off")) {
            Machine::estopOff();
            snprintf(out, outMax, "ok estop off");
        } else if (!strcmp(arg, "on")) {
            Machine::tripEstop();
            snprintf(out, outMax, "ok estop on");
        } else {
            snprintf(out, outMax, "ok state=%s", Machine::stateName());
        }
    }

    void power(const char *args, char *out, size_t outMax)
    {
        const char *a = skipSpaces(args);
        bool on = !strncmp(a, "on", 2) && (a[2] == '\0' || a[2] == ' ');
        bool off = !strncmp(a, "off", 3) && (a[3] == '\0' || a[3] == ' ');
        if (!on && !off) {
            snprintf(out, outMax, "err power on|off [z|x]");
            return;
        }
        const char *axisTok = skipSpaces(a + (on ? 2 : 3));
        bool z = !*axisTok || !strcmp(axisTok, "z");
        bool x = !*axisTok || !strcmp(axisTok, "x");
        if (!z && !x) {
            snprintf(out, outMax, "err power on|off [z|x]");
            return;
        }
        if (on && Machine::state() == Machine::State::EStop) {
            snprintf(out, outMax, "err busy: estop");
            return;
        }
        if (z) {
            Motion::setAxisEnabled(Stepper::Axis::Z, on);
        }
        if (x) {
            Motion::setAxisEnabled(Stepper::Axis::X, on);
        }
        snprintf(out, outMax, "ok power %s", on ? "on" : "off");
    }

    void commands(const char *, char *out, size_t outMax);

    const CommandDef COMMANDS[] = {
        {"ping", "ping", "replies pong", ping},
        {"reboot", "reboot", "restarts the controller", reboot},
        {"estop", "estop | estop on | estop off", "machine state query / trips the estop / clears the estop", estop},
        {"power", "power on|off [z|x]", "motors on or off, both axes without an axis", power},
        {"wifi",
         "wifi status|list|scan|on|off|forget | add <name> [pass] | ssid <name> [pass] | "
         "del <name|idx> | use <name|idx> | ap on|off | fallback on|off",
         "WiFi AP+STA control", wifi},
        {"settings", "settings get | set <field> <value> | set-all <json> | reset", "machine config storage", settings},
        {"jog", "jog step [<deci-microns>|next] | z|x +|- [hold|release] | resolution <multiplier> | speed <steps/sec|0> | status",
         "jog buttons (tap = one step, repeated hold = continuous, release = stop), handwheel resolution/speed", jog},
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
        {"cal", "cal count", "calibration: raw spindle count", cal},
        {"info", "info", "firmware version, target, reset cause, uptime, OTA image confirmed", info},
#ifdef CONFIG_SCREWSYNC_DISPLAY_IMAGE
        {"display", "display status | info | flash",
         "display link / stored display .tft: size and crc / flashes it to the display", display},
#else
        {"display", "display status", "display link", display},
#endif
        {"pins", "pins [reset]", "pin map in use (saved or board defaults) / saves the board defaults, applies after reboot",
         pins},
        {"units", "units [mm|inch]", "display units", units},
        {"ws", "ws list | kill <n>", "web UI WebSocket clients: numbered list / closes one", ws},
        {"o<name>", "o<name> call [<arg>]...", "runs a routine as a G-code job (args: [expressions])", nullptr},
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

    bool startsMotion(const char *token, size_t tokLen, const char *args)
    {
        args = skipSpaces(args);
        if (tokLen == 5 && !strncmp(token, "start", 5)) {
            return true;
        }
        if (tokLen == 3 && !strncmp(token, "jog", 3)) {
            return (args[0] == 'z' || args[0] == 'x') && args[1] == ' ';
        }
        return false;
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

        Machine::State st = Machine::state();
        if (st == Machine::State::Flashing) {
            snprintf(out, outMax, "err busy: flashing");
            return;
        }

        const char *pinsFault = Pins::fault();
        bool ui = *a == Gcode::MDI_UI_MARKER;
        const char *gcode = ui ? a + 1 : a;
        bool isMdi = Gcode::isMdiLine(gcode);
        bool isCall = (*a == 'o' || *a == 'O') && a[1] == '<';
        bool isJog = tokLen == 3 && !strncmp(a, "jog", 3);
        if (st == Machine::State::EStop && (isMdi || isCall || isJog || startsMotion(a, tokLen, a + tokLen))) {
            snprintf(out, outMax, "err busy: estop");
            return;
        }
        if (isMdi) {
            const char *refusal = pinsFault ? pinsFault : Mode::mdi(gcode, ui);
            if (refusal) {
                snprintf(out, outMax, "err %s%s", pinsFault ? "pins: " : "", refusal);
            } else {
                snprintf(out, outMax, "ok");
            }
            return;
        }
        if (isCall) {
            const char *refusal = pinsFault ? pinsFault : Mode::call(a);
            if (refusal) {
                snprintf(out, outMax, "err %s%s", pinsFault ? "pins: " : "", refusal);
            } else {
                snprintf(out, outMax, "ok");
            }
            return;
        }
        if (pinsFault && startsMotion(a, tokLen, a + tokLen)) {
            snprintf(out, outMax, "err pins: %s", pinsFault);
            return;
        }

        for (const CommandDef &c : COMMANDS) {
            if (c.handler && strlen(c.token) == tokLen && !strncmp(a, c.token, tokLen)) {
                c.handler(a + tokLen, out, outMax);
                return;
            }
        }
        snprintf(out, outMax, "err unknown command '%.*s'", (int)tokLen, a);
    }
}
