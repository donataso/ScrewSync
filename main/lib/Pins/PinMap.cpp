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
#include "PinMap.hpp"

#include <stdio.h>
#include <string.h>

namespace PinMap
{
    const FnDef FUNCTIONS[FN_COUNT] = {
        {"zMotor", "Z motor", 3, {"ENA", "DIR", "STEP"}, Dir::Out, true, false, false},
        {"xMotor", "X motor", 3, {"ENA", "DIR", "STEP"}, Dir::Out, false, false, false},
        {"spindle", "Spindle encoder", 2, {"A", "B"}, Dir::In, true, true, false},
        {"zHandwheel", "Z handwheel", 2, {"A", "B"}, Dir::In, false, true, true},
        {"xHandwheel", "X handwheel", 2, {"A", "B"}, Dir::In, false, true, true},
        {"probe", "Probe", 1, {"SIG"}, Dir::In, false, false, false},
    };

    const FnDef &def(Fn fn)
    {
        return FUNCTIONS[(size_t)fn];
    }

    int findFn(const char *id)
    {
        for (size_t i = 0; i < FN_COUNT; i++) {
            if (id && !strcmp(FUNCTIONS[i].id, id)) {
                return (int)i;
            }
        }
        return -1;
    }

    int findConnector(const Board &board, const char *id)
    {
        for (size_t i = 0; i < board.connectorCount; i++) {
            if (id && !strcmp(board.connectors[i].id, id)) {
                return (int)i;
            }
        }
        return -1;
    }

    int gpioOf(const Board &board, const Map &map, Fn fn, size_t signal)
    {
        const Assignment &a = map.fn[(size_t)fn];
        if (!a.used || signal >= def(fn).signalCount || a.connector >= board.connectorCount) {
            return -1;
        }
        const Connector &c = board.connectors[a.connector];
        size_t index = a.start + signal;
        return index < c.pinCount && c.pins[index] >= 0 ? c.pins[index] : -1;
    }

    int pcntUnit(const Map &map, Fn fn)
    {
        int unit = 0;
        for (size_t i = 0; i < FN_COUNT; i++) {
            const Assignment &a = map.fn[i];
            if (!a.used || a.encoder != Encoder::Hw) {
                continue;
            }
            int u = (Fn)i == Fn::Spindle ? 0 : ++unit;
            if ((Fn)i == fn) {
                return u;
            }
        }
        return -1;
    }

    bool isReserved(const Board &board, int gpio)
    {
        for (size_t i = 0; i < board.reservedCount; i++) {
            if (board.reserved[i] == gpio) {
                return true;
            }
        }
        return false;
    }

    bool fromGpios(const Board &board, Fn fn, const int32_t *gpios, size_t count, Assignment &out)
    {
        const FnDef &d = def(fn);
        if (count != d.signalCount) {
            return false;
        }
        for (size_t c = 0; c < board.connectorCount; c++) {
            const Connector &con = board.connectors[c];
            for (size_t start = 0; start + count <= con.pinCount; start++) {
                bool match = true;
                for (size_t i = 0; i < count && match; i++) {
                    match = con.pins[start + i] == gpios[i];
                }
                if (!match) {
                    continue;
                }
                out = {};
                out.used = true;
                out.connector = (uint8_t)c;
                out.start = (uint8_t)start;
                out.encoder = d.hwEncoder ? Encoder::Hw : (d.swEncoder ? Encoder::Sw : Encoder::None);
                for (size_t i = 0; i < count; i++) {
                    out.options[i] = {false, d.dir == Dir::In ? Pull::Up : Pull::None};
                }
                return true;
            }
        }
        return false;
    }

    const char *validateName(const char *name)
    {
        size_t len = strlen(name);
        if (len > CONNECTOR_NAME_MAX) {
            return "too long";
        }
        if (len && (name[0] == ' ' || name[len - 1] == ' ')) {
            return "starts or ends with a space";
        }
        for (size_t i = 0; i < len; i++) {
            if ((uint8_t)name[i] < 32 || name[i] == 127) {
                return "has control characters";
            }
        }
        return nullptr;
    }

    const char *validate(const Board &board, const Map &map, char *err, size_t errMax)
    {
        int owner[256];
        for (int &o : owner) {
            o = -1;
        }
        int hw = 0;
        for (size_t i = 0; i < FN_COUNT; i++) {
            const FnDef &d = FUNCTIONS[i];
            const Assignment &a = map.fn[i];
            if (!a.used) {
                if (d.required) {
                    snprintf(err, errMax, "%s has no pins", d.label);
                    return err;
                }
                continue;
            }
            if (a.connector >= board.connectorCount) {
                snprintf(err, errMax, "%s: unknown connector", d.label);
                return err;
            }
            for (size_t s = 0; s < d.signalCount; s++) {
                int gpio = gpioOf(board, map, (Fn)i, s);
                if (gpio < 0 || gpio > 255) {
                    snprintf(err, errMax, "%s: %s is not on a GPIO pin", d.label, d.signals[s]);
                    return err;
                }
                if (isReserved(board, gpio)) {
                    snprintf(err, errMax, "%s: GPIO %d is reserved", d.label, gpio);
                    return err;
                }
                if (owner[gpio] >= 0) {
                    snprintf(err, errMax, "GPIO %d is used by %s and %s", gpio, FUNCTIONS[owner[gpio]].label, d.label);
                    return err;
                }
                owner[gpio] = (int)i;
                if ((uint8_t)a.options[s].pull > (uint8_t)Pull::Down) {
                    snprintf(err, errMax, "%s: bad pull", d.label);
                    return err;
                }
                if (d.dir == Dir::Out && a.options[s].pull != Pull::None) {
                    snprintf(err, errMax, "%s: outputs have no pull", d.label);
                    return err;
                }
            }
            bool encoderOk = (d.hwEncoder || d.swEncoder) ? (a.encoder == Encoder::Hw && d.hwEncoder) ||
                                                                (a.encoder == Encoder::Sw && d.swEncoder)
                                                          : a.encoder == Encoder::None;
            if (!encoderOk) {
                snprintf(err, errMax, "%s: encoder type not allowed", d.label);
                return err;
            }
            if (a.encoder == Encoder::Hw) {
                hw++;
            }
        }
        if (hw > board.pcntUnits) {
            snprintf(err, errMax, "%d hardware encoders, but the board has %d PCNT units", hw, board.pcntUnits);
            return err;
        }
        for (size_t c = 0; c < MAX_CONNECTORS; c++) {
            const char *name = map.names[c];
            if (!memchr(name, '\0', CONNECTOR_NAME_MAX + 1)) {
                snprintf(err, errMax, "connector name too long");
                return err;
            }
            if (!name[0]) {
                continue;
            }
            const char *why = c < board.connectorCount ? validateName(name) : "unknown connector";
            if (why) {
                snprintf(err, errMax, "connector name \"%s\": %s", name, why);
                return err;
            }
        }
        return nullptr;
    }
}
