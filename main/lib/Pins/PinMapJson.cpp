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
#include "PinMapJson.hpp"

#include <math.h>
#include <stdio.h>
#include <string.h>
#include <zephyr/data/json.h>
#include <zephyr/sys/util.h>

using namespace PinMap;

namespace
{
    struct JSignal
    {
        bool invert;
        const char *pull;
    };

    struct JFunction
    {
        const char *id;
        const char *connector;
        int32_t start;
        const char *encoder;
        JSignal signals[MAX_SIGNALS];
        size_t signalCount;
        double filterUs;
    };

    struct JName
    {
        const char *connector;
        const char *name;
    };

    struct JReserved
    {
        int32_t gpio;
        const char *note;
    };

    struct JPpr
    {
        const char *fn;
        double ppr;
    };

    struct JConnector
    {
        const char *id;
        const char *title;
        int32_t row;
        int32_t col;
        int32_t pins[MAX_PINS];
        size_t pinCount;
    };

    struct JBoard
    {
        const char *name;
        int32_t pcntUnits;
        double maxHwFilterUs;
        double maxSwFilterUs;
        double tickUs;
        JPpr ppr[FN_COUNT];
        size_t pprCount;
        int32_t strapping[MAX_STRAPPING];
        size_t strappingCount;
        JReserved reserved[MAX_RESERVED];
        size_t reservedCount;
        JConnector connectors[MAX_CONNECTORS];
        size_t connectorCount;
    };

    struct JUpdate
    {
        JFunction functions[MAX_ASSIGNMENTS];
        size_t functionCount;
        JName names[MAX_CONNECTORS];
        size_t nameCount;
    };

    struct JDoc
    {
        JBoard board;
        JFunction functions[MAX_ASSIGNMENTS];
        size_t functionCount;
        JName names[MAX_CONNECTORS];
        size_t nameCount;
    };

    const struct json_obj_descr SIGNAL_DESCR[] = {
        JSON_OBJ_DESCR_PRIM(JSignal, invert, JSON_TOK_TRUE),
        JSON_OBJ_DESCR_PRIM(JSignal, pull, JSON_TOK_STRING),
    };

    const struct json_obj_descr FUNCTION_DESCR[] = {
        JSON_OBJ_DESCR_PRIM(JFunction, id, JSON_TOK_STRING),
        JSON_OBJ_DESCR_PRIM(JFunction, connector, JSON_TOK_STRING),
        JSON_OBJ_DESCR_PRIM(JFunction, start, JSON_TOK_NUMBER),
        JSON_OBJ_DESCR_PRIM(JFunction, encoder, JSON_TOK_STRING),
        JSON_OBJ_DESCR_OBJ_ARRAY(JFunction, signals, MAX_SIGNALS, signalCount, SIGNAL_DESCR,
                                 ARRAY_SIZE(SIGNAL_DESCR)),
        JSON_OBJ_DESCR_PRIM(JFunction, filterUs, JSON_TOK_DOUBLE_FP),
    };

    const struct json_obj_descr NAME_DESCR[] = {
        JSON_OBJ_DESCR_PRIM(JName, connector, JSON_TOK_STRING),
        JSON_OBJ_DESCR_PRIM(JName, name, JSON_TOK_STRING),
    };

    const struct json_obj_descr RESERVED_DESCR[] = {
        JSON_OBJ_DESCR_PRIM(JReserved, gpio, JSON_TOK_NUMBER),
        JSON_OBJ_DESCR_PRIM(JReserved, note, JSON_TOK_STRING),
    };

    const struct json_obj_descr PPR_DESCR[] = {
        JSON_OBJ_DESCR_PRIM(JPpr, fn, JSON_TOK_STRING),
        JSON_OBJ_DESCR_PRIM(JPpr, ppr, JSON_TOK_DOUBLE_FP),
    };

    const struct json_obj_descr CONNECTOR_DESCR[] = {
        JSON_OBJ_DESCR_PRIM(JConnector, id, JSON_TOK_STRING),
        JSON_OBJ_DESCR_PRIM(JConnector, title, JSON_TOK_STRING),
        JSON_OBJ_DESCR_PRIM(JConnector, row, JSON_TOK_NUMBER),
        JSON_OBJ_DESCR_PRIM(JConnector, col, JSON_TOK_NUMBER),
        JSON_OBJ_DESCR_ARRAY(JConnector, pins, MAX_PINS, pinCount, JSON_TOK_NUMBER),
    };

    const struct json_obj_descr BOARD_DESCR[] = {
        JSON_OBJ_DESCR_PRIM(JBoard, name, JSON_TOK_STRING),
        JSON_OBJ_DESCR_PRIM(JBoard, pcntUnits, JSON_TOK_NUMBER),
        JSON_OBJ_DESCR_PRIM(JBoard, maxHwFilterUs, JSON_TOK_DOUBLE_FP),
        JSON_OBJ_DESCR_PRIM(JBoard, maxSwFilterUs, JSON_TOK_DOUBLE_FP),
        JSON_OBJ_DESCR_PRIM(JBoard, tickUs, JSON_TOK_DOUBLE_FP),
        JSON_OBJ_DESCR_OBJ_ARRAY(JBoard, ppr, FN_COUNT, pprCount, PPR_DESCR, ARRAY_SIZE(PPR_DESCR)),
        JSON_OBJ_DESCR_ARRAY(JBoard, strapping, MAX_STRAPPING, strappingCount, JSON_TOK_NUMBER),
        JSON_OBJ_DESCR_OBJ_ARRAY(JBoard, reserved, MAX_RESERVED, reservedCount, RESERVED_DESCR,
                                 ARRAY_SIZE(RESERVED_DESCR)),
        JSON_OBJ_DESCR_OBJ_ARRAY(JBoard, connectors, MAX_CONNECTORS, connectorCount, CONNECTOR_DESCR,
                                 ARRAY_SIZE(CONNECTOR_DESCR)),
    };

    const struct json_obj_descr DOC_DESCR[] = {
        JSON_OBJ_DESCR_OBJECT(JDoc, board, BOARD_DESCR),
        JSON_OBJ_DESCR_OBJ_ARRAY(JDoc, functions, MAX_ASSIGNMENTS, functionCount, FUNCTION_DESCR, ARRAY_SIZE(FUNCTION_DESCR)),
        JSON_OBJ_DESCR_OBJ_ARRAY(JDoc, names, MAX_CONNECTORS, nameCount, NAME_DESCR, ARRAY_SIZE(NAME_DESCR)),
    };

    const struct json_obj_descr UPDATE_DESCR[] = {
        JSON_OBJ_DESCR_OBJ_ARRAY(JUpdate, functions, MAX_ASSIGNMENTS, functionCount, FUNCTION_DESCR,
                                 ARRAY_SIZE(FUNCTION_DESCR)),
        JSON_OBJ_DESCR_OBJ_ARRAY(JUpdate, names, MAX_CONNECTORS, nameCount, NAME_DESCR, ARRAY_SIZE(NAME_DESCR)),
    };

    struct JFnDef
    {
        const char *id;
        const char *label;
        const char *group;
        const char *signals[MAX_SIGNALS];
        size_t signalCount;
        const char *dir;
        bool required;
        bool exclusive;
        bool activeLow;
        const char *encoders[2];
        size_t encoderCount;
        double filterUs;
        int32_t maxRpm;
    };

    struct JFnDoc
    {
        JFnDef functions[FN_COUNT];
        size_t functionCount;
    };

    const struct json_obj_descr FN_DEF_DESCR[] = {
        JSON_OBJ_DESCR_PRIM(JFnDef, id, JSON_TOK_STRING),
        JSON_OBJ_DESCR_PRIM(JFnDef, label, JSON_TOK_STRING),
        JSON_OBJ_DESCR_PRIM(JFnDef, group, JSON_TOK_STRING),
        JSON_OBJ_DESCR_ARRAY(JFnDef, signals, MAX_SIGNALS, signalCount, JSON_TOK_STRING),
        JSON_OBJ_DESCR_PRIM(JFnDef, dir, JSON_TOK_STRING),
        JSON_OBJ_DESCR_PRIM(JFnDef, required, JSON_TOK_TRUE),
        JSON_OBJ_DESCR_PRIM(JFnDef, exclusive, JSON_TOK_TRUE),
        JSON_OBJ_DESCR_PRIM(JFnDef, activeLow, JSON_TOK_TRUE),
        JSON_OBJ_DESCR_ARRAY(JFnDef, encoders, 2, encoderCount, JSON_TOK_STRING),
        JSON_OBJ_DESCR_PRIM(JFnDef, filterUs, JSON_TOK_DOUBLE_FP),
        JSON_OBJ_DESCR_PRIM(JFnDef, maxRpm, JSON_TOK_NUMBER),
    };

    const struct json_obj_descr FN_DOC_DESCR[] = {
        JSON_OBJ_DESCR_OBJ_ARRAY(JFnDoc, functions, FN_COUNT, functionCount, FN_DEF_DESCR, ARRAY_SIZE(FN_DEF_DESCR)),
    };

    const char *const PULLS[] = {"none", "up", "down"};
    const char *const ENCODERS[] = {"", "hw", "sw"};

    JDoc g_doc;
    JFnDoc g_fnDoc;
    JUpdate g_update;

    int indexOf(const char *const *list, size_t count, const char *value)
    {
        for (size_t i = 0; i < count; i++) {
            if (value && !strcmp(list[i], value)) {
                return (int)i;
            }
        }
        return -1;
    }

    const char *fail(char *err, size_t errMax, const char *msg, const char *what)
    {
        snprintf(err, errMax, "%s%s%s", msg, what ? ": " : "", what ? what : "");
        return err;
    }
}

namespace PinMapJson
{
    int encode(const Board &board, const Map &map, char *out, size_t outMax)
    {
        memset(&g_doc, 0, sizeof(g_doc));
        JBoard &b = g_doc.board;
        b.name = board.name;
        b.pcntUnits = board.pcntUnits;
        b.maxHwFilterUs = MAX_HW_FILTER_NS / 1000.0;
        b.maxSwFilterUs = MAX_SW_FILTER_NS / 1000.0;
        b.tickUs = board.tickNs / 1000.0;
        for (size_t i = 0; i < FN_COUNT; i++) {
            if (board.ppr[i] > 0) {
                b.ppr[b.pprCount++] = {FUNCTIONS[i].id, board.ppr[i]};
            }
        }
        b.strappingCount = board.strappingCount;
        for (size_t i = 0; i < board.strappingCount; i++) {
            b.strapping[i] = board.strapping[i];
        }
        b.reservedCount = board.reservedCount;
        for (size_t i = 0; i < board.reservedCount; i++) {
            b.reserved[i] = {board.reserved[i], board.reservedNotes[i] ? board.reservedNotes[i] : ""};
        }
        b.connectorCount = board.connectorCount;
        for (size_t i = 0; i < board.connectorCount; i++) {
            const Connector &c = board.connectors[i];
            JConnector &j = b.connectors[i];
            j.id = c.id;
            j.title = c.title;
            j.row = c.row;
            j.col = c.col;
            j.pinCount = c.pinCount;
            for (size_t p = 0; p < c.pinCount; p++) {
                j.pins[p] = c.pins[p];
            }
        }
        for (size_t i = 0; i < map.count && i < MAX_ASSIGNMENTS; i++) {
            const Assignment &a = map.list[i];
            if ((size_t)a.fn >= FN_COUNT || a.connector >= board.connectorCount) {
                continue;
            }
            const FnDef &d = def(a.fn);
            JFunction &f = g_doc.functions[g_doc.functionCount++];
            f.id = d.id;
            f.connector = board.connectors[a.connector].id;
            f.start = a.start;
            f.encoder = ENCODERS[(size_t)a.encoder <= 2 ? (size_t)a.encoder : 0];
            f.filterUs = a.filterNs / 1000.0;
            f.signalCount = d.signalCount;
            for (size_t s = 0; s < f.signalCount; s++) {
                f.signals[s] = {a.options[s].invert, PULLS[(size_t)a.options[s].pull <= 2 ? (size_t)a.options[s].pull : 0]};
            }
        }
        for (size_t c = 0; c < board.connectorCount; c++) {
            if (map.names[c][0]) {
                g_doc.names[g_doc.nameCount++] = {board.connectors[c].id, map.names[c]};
            }
        }
        int rc = json_obj_encode_buf(DOC_DESCR, ARRAY_SIZE(DOC_DESCR), &g_doc, out, outMax);
        return rc == 0 ? (int)strlen(out) : -1;
    }

    const char *decode(const Board &board, char *json, size_t len, Map &out, char *err, size_t errMax)
    {
        memset(&g_update, 0, sizeof(g_update));
        for (JFunction &f : g_update.functions) {
            f.filterUs = -1;
        }
        int64_t mask = json_obj_parse(json, len, UPDATE_DESCR, ARRAY_SIZE(UPDATE_DESCR), &g_update);
        if (mask < 0 || !(mask & 1)) {
            return fail(err, errMax, "bad json", nullptr);
        }
        memset(&out, 0, sizeof(out));
        for (size_t i = 0; i < g_update.functionCount; i++) {
            const JFunction &f = g_update.functions[i];
            int fn = findFn(f.id);
            if (fn < 0) {
                return fail(err, errMax, "unknown function", f.id);
            }
            int c = findConnector(board, f.connector);
            if (c < 0) {
                return fail(err, errMax, "unknown connector", f.connector);
            }
            if (f.start < 0 || f.start >= (int32_t)MAX_PINS) {
                return fail(err, errMax, "bad start", f.id);
            }
            if (f.signalCount != FUNCTIONS[fn].signalCount) {
                return fail(err, errMax, "wrong signal count", f.id);
            }
            int enc = indexOf(ENCODERS, ARRAY_SIZE(ENCODERS), f.encoder ? f.encoder : "");
            if (enc < 0) {
                return fail(err, errMax, "bad encoder type", f.id);
            }
            Assignment a = {};
            a.fn = (Fn)fn;
            a.connector = (uint8_t)c;
            a.start = (uint8_t)f.start;
            a.encoder = (Encoder)enc;
            if (!(f.filterUs <= UINT32_MAX / 1000.0)) {
                return fail(err, errMax, "bad filter", f.id);
            }
            a.filterNs = f.filterUs < 0 ? FUNCTIONS[fn].filterNs : (uint32_t)lround(f.filterUs * 1000.0);
            for (size_t s = 0; s < f.signalCount; s++) {
                int pull = indexOf(PULLS, ARRAY_SIZE(PULLS), f.signals[s].pull ? f.signals[s].pull : "none");
                if (pull < 0) {
                    return fail(err, errMax, "bad pull", f.id);
                }
                a.options[s] = {f.signals[s].invert, (Pull)pull};
            }
            add(out, a);
        }
        for (size_t i = 0; i < g_update.nameCount; i++) {
            const JName &n = g_update.names[i];
            int c = findConnector(board, n.connector);
            if (c < 0) {
                return fail(err, errMax, "unknown connector", n.connector);
            }
            if (!n.name || strlen(n.name) > CONNECTOR_NAME_MAX) {
                return fail(err, errMax, "bad connector name", n.connector);
            }
            strcpy(out.names[c], n.name);
        }
        return validate(board, out, err, errMax);
    }

    int encodeFunctions(char *out, size_t outMax)
    {
        memset(&g_fnDoc, 0, sizeof(g_fnDoc));
        g_fnDoc.functionCount = FN_COUNT;
        for (size_t i = 0; i < FN_COUNT; i++) {
            const FnDef &d = FUNCTIONS[i];
            JFnDef &j = g_fnDoc.functions[i];
            j.id = d.id;
            j.label = d.label;
            j.group = d.group;
            j.signalCount = d.signalCount;
            for (size_t s = 0; s < d.signalCount; s++) {
                j.signals[s] = d.signals[s];
            }
            j.dir = d.dir == Dir::In ? "in" : "out";
            j.required = d.required;
            j.exclusive = d.exclusive;
            j.activeLow = d.activeLow;
            if (d.hwEncoder) {
                j.encoders[j.encoderCount++] = "hw";
            }
            if (d.swEncoder) {
                j.encoders[j.encoderCount++] = "sw";
            }
            j.filterUs = d.filterNs / 1000.0;
            j.maxRpm = (int32_t)d.maxRpm;
        }
        int rc = json_obj_encode_buf(FN_DOC_DESCR, ARRAY_SIZE(FN_DOC_DESCR), &g_fnDoc, out, outMax);
        return rc == 0 ? (int)strlen(out) : -1;
    }
}
