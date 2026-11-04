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
#include "ThreadStore.hpp"
#include "Machine.hpp"
#include "Mode.hpp"
#include <string.h>
#include <zephyr/data/json.h>
#include <zephyr/sys/util.h>

#ifdef CONFIG_ESP_SPIRAM
#define EXT_RAM_BSS __attribute__((section(".ext_ram.bss")))
#else
#define EXT_RAM_BSS
#endif

namespace
{
    using ThreadTable::Entry;
    using ThreadTable::Group;
    using ThreadTable::Table;

    Table g_scratch EXT_RAM_BSS;
    ThreadStore::JsonTable g_json EXT_RAM_BSS;

    const struct json_obj_descr GROUP_DESCR[] = {
        JSON_OBJ_DESCR_PRIM(ThreadStore::JsonGroup, name, JSON_TOK_STRING),
        JSON_OBJ_DESCR_PRIM(ThreadStore::JsonGroup, depthExternal, JSON_TOK_FLOAT_FP),
        JSON_OBJ_DESCR_PRIM(ThreadStore::JsonGroup, depthInternal, JSON_TOK_FLOAT_FP),
        JSON_OBJ_DESCR_PRIM(ThreadStore::JsonGroup, taper, JSON_TOK_FLOAT_FP),
        JSON_OBJ_DESCR_PRIM(ThreadStore::JsonGroup, flankAngle, JSON_TOK_FLOAT_FP),
    };

    const struct json_obj_descr ENTRY_DESCR[] = {
        JSON_OBJ_DESCR_PRIM(ThreadStore::JsonEntry, group, JSON_TOK_STRING),
        JSON_OBJ_DESCR_PRIM(ThreadStore::JsonEntry, designation, JSON_TOK_STRING),
        JSON_OBJ_DESCR_PRIM(ThreadStore::JsonEntry, majorDu, JSON_TOK_NUMBER),
        JSON_OBJ_DESCR_PRIM(ThreadStore::JsonEntry, pitchDu, JSON_TOK_NUMBER),
    };

    const struct json_obj_descr OVERRIDE_DESCR[] = {
        JSON_OBJ_DESCR_PRIM(ThreadStore::JsonOverride, group, JSON_TOK_STRING),
        JSON_OBJ_DESCR_PRIM(ThreadStore::JsonOverride, designation, JSON_TOK_STRING),
        JSON_OBJ_DESCR_PRIM(ThreadStore::JsonOverride, depthExternalDu, JSON_TOK_NUMBER),
        JSON_OBJ_DESCR_PRIM(ThreadStore::JsonOverride, depthInternalDu, JSON_TOK_NUMBER),
        JSON_OBJ_DESCR_PRIM(ThreadStore::JsonOverride, hasTaper, JSON_TOK_TRUE),
        JSON_OBJ_DESCR_PRIM(ThreadStore::JsonOverride, taper, JSON_TOK_FLOAT_FP),
    };

    bool copyName(char *dst, size_t dstMax, const char *src)
    {
        if (!src || strlen(src) >= dstMax) {
            return false;
        }
        strcpy(dst, src);
        return true;
    }
}

namespace ThreadStore
{
    const struct json_obj_descr JSON_TABLE_DESCR[] = {
        JSON_OBJ_DESCR_OBJ_ARRAY(ThreadStore::JsonTable, groups, ThreadTable::MAX_GROUPS, groupCount, GROUP_DESCR,
                                 ARRAY_SIZE(GROUP_DESCR)),
        JSON_OBJ_DESCR_OBJ_ARRAY(ThreadStore::JsonTable, entries, ThreadTable::MAX_ENTRIES, entryCount, ENTRY_DESCR,
                                 ARRAY_SIZE(ENTRY_DESCR)),
        JSON_OBJ_DESCR_OBJ_ARRAY(ThreadStore::JsonTable, overrides, ThreadTable::MAX_ENTRIES, overrideCount,
                                 OVERRIDE_DESCR, ARRAY_SIZE(OVERRIDE_DESCR)),
    };

    void toJson(const Table &table, JsonTable &json)
    {
        json.groupCount = table.groupCount;
        for (size_t i = 0; i < table.groupCount; i++)
        {
            const Group &g = table.groups[i];
            json.groups[i] = {g.name, g.depthExternal, g.depthInternal, g.taper, g.flankAngle};
        }
        json.entryCount = table.entryCount;
        for (size_t i = 0; i < table.entryCount; i++)
        {
            const Entry &e = table.entries[i];
            json.entries[i] = {table.groups[e.group].name, e.designation, e.majorDu, e.pitchDu};
        }
        json.overrideCount = 0;
        for (size_t i = 0; i < table.entryCount; i++)
        {
            const Entry &e = table.entries[i];
            if (e.depthExternalDu || e.depthInternalDu || e.hasTaper) {
                json.overrides[json.overrideCount++] = {table.groups[e.group].name, e.designation, e.depthExternalDu,
                                                        e.depthInternalDu, e.hasTaper, e.taper};
            }
        }
    }

    const char *fromJson(const JsonTable &json, Table &out)
    {
        out.groupCount = json.groupCount;
        for (size_t i = 0; i < json.groupCount; i++)
        {
            const JsonGroup &j = json.groups[i];
            Group &g = out.groups[i];
            if (!copyName(g.name, sizeof(g.name), j.name)) {
                return "bad group name";
            }
            g.depthExternal = j.depthExternal;
            g.depthInternal = j.depthInternal;
            g.taper = j.taper;
            g.flankAngle = j.flankAngle;
        }
        out.entryCount = json.entryCount;
        for (size_t i = 0; i < json.entryCount; i++)
        {
            const JsonEntry &j = json.entries[i];
            Entry &e = out.entries[i];
            int g = j.group ? ThreadTable::findGroup(out, j.group) : -1;
            if (g < 0) {
                return "entry without group";
            }
            e = {};
            if (!copyName(e.designation, sizeof(e.designation), j.designation)) {
                return "bad designation";
            }
            e.group = (uint8_t)g;
            e.majorDu = j.majorDu;
            e.pitchDu = j.pitchDu;
        }
        for (size_t i = 0; i < json.overrideCount; i++)
        {
            const JsonOverride &j = json.overrides[i];
            Entry *e = j.group && j.designation ? const_cast<Entry *>(ThreadTable::find(out, j.group, j.designation))
                                                : nullptr;
            if (!e) {
                return "override without thread";
            }
            e->depthExternalDu = j.depthExternalDu;
            e->depthInternalDu = j.depthInternalDu;
            e->hasTaper = j.hasTaper;
            e->taper = j.hasTaper ? j.taper : 0;
        }
        return ThreadTable::validate(out);
    }

    int toJson(char *out, size_t outMax)
    {
        snapshot(g_scratch);
        toJson(g_scratch, g_json);
        int rc = json_obj_encode_buf(JSON_TABLE_DESCR, ARRAY_SIZE(JSON_TABLE_DESCR), &g_json, out, outMax);
        return rc == 0 ? (int)strlen(out) : -1;
    }

    const char *fromJson(char *json, size_t len)
    {
        if (Machine::state() != Machine::State::Idle || Mode::running()) {
            return "machine not idle";
        }
        memset(&g_json, 0, sizeof(g_json));
        int64_t mask = json_obj_parse(json, len, JSON_TABLE_DESCR, ARRAY_SIZE(JSON_TABLE_DESCR), &g_json);
        if (mask < 0 || (mask & 0b11) != 0b11) {
            return "bad json";
        }
        const char *refusal = fromJson(g_json, g_scratch);
        if (!refusal) {
            replace(g_scratch);
        }
        return refusal;
    }
}
