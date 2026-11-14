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
    constexpr uint32_t ONE_APB_CYCLE_NS = 13;

#define SWITCH_IN(group, id, label, action) {id, label, group, 1, {"SIG"}, Dir::In, false, false, false, false, true, action, 0}
#define SWITCH(id, label, action) SWITCH_IN("Inputs", id, label, action)
#define MODE_SWITCH(id, label, action) SWITCH_IN("Modes", id, label, action)
    const FnDef FUNCTIONS[FN_COUNT] = {
        {"zMotor", "Z motor", "Motors", 3, {"ENA", "DIR", "STEP"}, Dir::Out, true, true, false, false, false, nullptr, 0},
        {"xMotor", "X motor", "Motors", 3, {"ENA", "DIR", "STEP"}, Dir::Out, false, true, false, false, false, nullptr, 0},
        {"spindle", "Spindle encoder", "Encoders", 2, {"A", "B"}, Dir::In, true, true, true, false, false, nullptr, ONE_APB_CYCLE_NS},
        {"zHandwheel", "Z handwheel", "Encoders", 2, {"A", "B"}, Dir::In, false, true, true, true, false, nullptr, MAX_HW_FILTER_NS},
        {"xHandwheel", "X handwheel", "Encoders", 2, {"A", "B"}, Dir::In, false, true, true, true, false, nullptr, MAX_HW_FILTER_NS},
        {"probe", "Probe", "Inputs", 1, {"SIG"}, Dir::In, false, false, false, false, false, nullptr, 0},
        SWITCH("startHere", "Start here", "start"),
        SWITCH("stop", "Stop", "stop"),
        SWITCH("setZero", "Set zero", "zero z;zero x"),
        SWITCH("setZeroZ", "Set zero Z", "zero z"),
        SWITCH("setZeroX", "Set zero X", "zero x"),
        SWITCH("goToZero", "Go to zero", "move zero"),
        SWITCH("goToZeroZ", "Go to zero Z", "move z to 0"),
        SWITCH("goToZeroX", "Go to zero X", "move x to 0"),
        SWITCH("feedReset", "Feed override reset", "override reset"),
        SWITCH("feedUp", "Feed +", "override +"),
        SWITCH("feedDown", "Feed -", "override -"),
        MODE_SWITCH("modeGear", "Mode gear", "mode gear"),
        MODE_SWITCH("modeAsync", "Mode async", "mode async"),
        MODE_SWITCH("modeTurn", "Mode turn", "mode turn"),
        MODE_SWITCH("modeThread", "Mode thread", "mode thread"),
        MODE_SWITCH("modePart", "Mode part", "mode part"),
        MODE_SWITCH("modeSlot", "Mode slot", "mode slot"),
        MODE_SWITCH("modeEllipse", "Mode ellipse", "mode ellipse"),
        MODE_SWITCH("modeGcode", "Mode G-code", "mode gcode"),
        MODE_SWITCH("modeProbe", "Mode probe", "mode probe"),
        {"running", "Running", "Outputs", 1, {"SIG"}, Dir::Out, false, false, false, false, false, nullptr, 0},
    };
#undef MODE_SWITCH
#undef SWITCH
#undef SWITCH_IN

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

    const Assignment *find(const Map &map, Fn fn, size_t nth)
    {
        for (size_t i = 0; i < map.count; i++) {
            if (map.list[i].fn == fn && nth-- == 0) {
                return &map.list[i];
            }
        }
        return nullptr;
    }

    size_t countOf(const Map &map, Fn fn)
    {
        size_t n = 0;
        for (size_t i = 0; i < map.count; i++) {
            n += map.list[i].fn == fn;
        }
        return n;
    }

    bool add(Map &map, const Assignment &a)
    {
        if (map.count >= MAX_ASSIGNMENTS) {
            return false;
        }
        map.list[map.count++] = a;
        return true;
    }

    int gpioOf(const Board &board, const Assignment &a, size_t signal)
    {
        if ((size_t)a.fn >= FN_COUNT || signal >= def(a.fn).signalCount || a.connector >= board.connectorCount) {
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
            const Assignment *a = find(map, (Fn)i);
            if (!a || a->encoder != Encoder::Hw) {
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
                out.fn = fn;
                out.connector = (uint8_t)c;
                out.start = (uint8_t)start;
                out.encoder = d.hwEncoder ? Encoder::Hw : (d.swEncoder ? Encoder::Sw : Encoder::None);
                out.filterNs = d.filterNs;
                for (size_t i = 0; i < count; i++) {
                    out.options[i] = {d.activeLow, d.dir == Dir::In ? Pull::Up : Pull::None};
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

    const char *checkAssignment(const Board &board, const Assignment *kept, size_t keptCount, const Assignment &a,
                                char *err, size_t errMax)
    {
        if ((size_t)a.fn >= FN_COUNT) {
            snprintf(err, errMax, "unknown function");
            return err;
        }
        const FnDef &d = def(a.fn);
        if (a.connector >= board.connectorCount) {
            snprintf(err, errMax, "%s: unknown connector", d.label);
            return err;
        }
        int hw = a.encoder == Encoder::Hw;
        for (size_t k = 0; k < keptCount; k++) {
            if (d.exclusive && kept[k].fn == a.fn) {
                snprintf(err, errMax, "%s is given twice", d.label);
                return err;
            }
            hw += kept[k].encoder == Encoder::Hw;
        }
        for (size_t s = 0; s < d.signalCount; s++) {
            int gpio = gpioOf(board, a, s);
            if (gpio < 0) {
                snprintf(err, errMax, "%s: %s is not on a GPIO pin", d.label, d.signals[s]);
                return err;
            }
            if (isReserved(board, gpio)) {
                snprintf(err, errMax, "%s: GPIO %d is reserved", d.label, gpio);
                return err;
            }
            for (size_t k = 0; k < keptCount; k++) {
                for (size_t t = 0; t < def(kept[k].fn).signalCount; t++) {
                    if (gpioOf(board, kept[k], t) == gpio) {
                        snprintf(err, errMax, "GPIO %d is used by %s and %s", gpio, def(kept[k].fn).label, d.label);
                        return err;
                    }
                }
            }
            if ((uint8_t)a.options[s].pull > (uint8_t)Pull::Down) {
                snprintf(err, errMax, "%s: bad pull", d.label);
                return err;
            }
            if (d.dir == Dir::Out && a.options[s].pull != Pull::None) {
                snprintf(err, errMax, "%s: outputs have no pull", d.label);
                return err;
            }
        }
        bool encoderOk = (d.hwEncoder || d.swEncoder)
                             ? (a.encoder == Encoder::Hw && d.hwEncoder) || (a.encoder == Encoder::Sw && d.swEncoder)
                             : a.encoder == Encoder::None;
        if (!encoderOk) {
            snprintf(err, errMax, "%s: encoder type not allowed", d.label);
            return err;
        }
        uint32_t maxFilterNs = a.encoder == Encoder::Sw ? MAX_SW_FILTER_NS : MAX_HW_FILTER_NS;
        if (a.filterNs > maxFilterNs) {
            snprintf(err, errMax, "%s: filter above %lu ns", d.label, (unsigned long)maxFilterNs);
            return err;
        }
        if (a.encoder == Encoder::Hw && hw > board.pcntUnits) {
            snprintf(err, errMax, "%s: no free PCNT unit, the board has %d", d.label, board.pcntUnits);
            return err;
        }
        return nullptr;
    }

    const char *missingRequired(const Map &map, char *err, size_t errMax)
    {
        for (size_t i = 0; i < FN_COUNT; i++) {
            if (FUNCTIONS[i].required && !find(map, (Fn)i)) {
                snprintf(err, errMax, "%s has no pins", FUNCTIONS[i].label);
                return err;
            }
        }
        return nullptr;
    }

    const char *validate(const Board &board, const Map &map, char *err, size_t errMax)
    {
        if (map.count > MAX_ASSIGNMENTS) {
            snprintf(err, errMax, "too many functions");
            return err;
        }
        for (size_t i = 0; i < map.count; i++) {
            if (checkAssignment(board, map.list, i, map.list[i], err, errMax)) {
                return err;
            }
        }
        if (missingRequired(map, err, errMax)) {
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

    void keepValid(const Board &board, const Map &in, Map &out, Warn warn)
    {
        char err[ERROR_MAX];
        out = {};
        size_t count = in.count < MAX_ASSIGNMENTS ? in.count : MAX_ASSIGNMENTS;
        for (size_t i = 0; i < count; i++) {
            if (checkAssignment(board, out.list, out.count, in.list[i], err, sizeof(err))) {
                warn(err);
            } else {
                add(out, in.list[i]);
            }
        }
        for (size_t c = 0; c < board.connectorCount && c < MAX_CONNECTORS; c++) {
            const char *name = in.names[c];
            if (memchr(name, '\0', CONNECTOR_NAME_MAX + 1) && !validateName(name)) {
                strcpy(out.names[c], name);
            } else {
                warn("bad connector name");
            }
        }
    }

    void toStored(const Board &board, const Map &map, Stored &out)
    {
        memset(&out, 0, sizeof(out));
        out.magic = STORED_MAGIC;
        for (size_t i = 0; i < map.count && i < MAX_ASSIGNMENTS; i++) {
            const Assignment &a = map.list[i];
            if ((size_t)a.fn >= FN_COUNT || a.connector >= board.connectorCount) {
                continue;
            }
            StoredAssignment &s = out.assignments[out.assignmentCount++];
            strncpy(s.fn, def(a.fn).id, ID_MAX);
            strncpy(s.connector, board.connectors[a.connector].id, ID_MAX);
            s.start = a.start;
            s.encoder = (uint8_t)a.encoder;
            memcpy(s.options, a.options, sizeof(s.options));
            s.filterNs = a.filterNs;
        }
        for (size_t c = 0; c < board.connectorCount && c < MAX_CONNECTORS; c++) {
            if (map.names[c][0]) {
                StoredName &s = out.names[out.nameCount++];
                strncpy(s.connector, board.connectors[c].id, ID_MAX);
                strncpy(s.name, map.names[c], CONNECTOR_NAME_MAX);
            }
        }
    }

    bool fromStored(const Board &board, const Stored &in, Map &out, Warn warn)
    {
        memset(&out, 0, sizeof(out));
        if (in.magic != STORED_MAGIC || in.assignmentCount > MAX_ASSIGNMENTS || in.nameCount > MAX_CONNECTORS) {
            return false;
        }
        char msg[ERROR_MAX];
        for (size_t i = 0; i < in.assignmentCount; i++) {
            const StoredAssignment &s = in.assignments[i];
            char fnId[ID_MAX + 1] = {};
            char connectorId[ID_MAX + 1] = {};
            memcpy(fnId, s.fn, ID_MAX);
            memcpy(connectorId, s.connector, ID_MAX);
            int fn = findFn(fnId);
            int c = findConnector(board, connectorId);
            if (fn < 0 || c < 0 || s.encoder > (uint8_t)Encoder::Sw) {
                snprintf(msg, sizeof(msg), "%s on %s does not fit this firmware", fnId, connectorId);
                warn(msg);
                continue;
            }
            Assignment a = {(Fn)fn, (uint8_t)c, s.start, (Encoder)s.encoder, {}, s.filterNs};
            memcpy(a.options, s.options, sizeof(a.options));
            add(out, a);
        }
        for (size_t i = 0; i < in.nameCount; i++) {
            const StoredName &s = in.names[i];
            char connectorId[ID_MAX + 1] = {};
            memcpy(connectorId, s.connector, ID_MAX);
            int c = findConnector(board, connectorId);
            if (c >= 0) {
                memcpy(out.names[c], s.name, CONNECTOR_NAME_MAX);
                out.names[c][CONNECTOR_NAME_MAX] = '\0';
            }
        }
        return true;
    }
}
