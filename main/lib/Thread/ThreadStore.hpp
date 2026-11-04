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
#ifndef ThreadStore_h
#define ThreadStore_h

#include "ThreadTable.hpp"
#include <stddef.h>
#include <stdint.h>
#include <zephyr/data/json.h>

// The live thread table: built-in defaults until replaced (web editor),
// persisted in its own NVS namespace.
namespace ThreadStore
{
    struct Preset
    {
        int32_t pitchDu;
        int32_t majorDu;
        int32_t depthExternalDu;
        int32_t depthInternalDu;
        float taper;
        float flankAngle;
    };

    struct JsonGroup
    {
        const char *name;
        float depthExternal;
        float depthInternal;
        float taper;
        float flankAngle;
    };

    struct JsonEntry
    {
        const char *group;
        const char *designation;
        int32_t majorDu;
        int32_t pitchDu;
    };

    // Only entries with an override; depth 0 = group's.
    struct JsonOverride
    {
        const char *group;
        const char *designation;
        int32_t depthExternalDu;
        int32_t depthInternalDu;
        bool hasTaper;
        float taper;
    };

    struct JsonTable
    {
        JsonGroup groups[ThreadTable::MAX_GROUPS];
        size_t groupCount;
        JsonEntry entries[ThreadTable::MAX_ENTRIES];
        size_t entryCount;
        JsonOverride overrides[ThreadTable::MAX_ENTRIES];
        size_t overrideCount;
    };

    extern const struct json_obj_descr JSON_TABLE_DESCR[3];

    void begin();

    // Back to the built-in defaults.
    void reset();

    bool lookup(const char *group, const char *designation, Preset &out);

    // Comma-separated group names.
    void groups(char *out, size_t outMax);

    // Comma-separated designations of a group from offset, as many as fit.
    // next = offset to continue from, total when complete. False if no
    // such group.
    bool list(const char *group, size_t offset, char *out, size_t outMax, size_t &total, size_t &next);

    void snapshot(ThreadTable::Table &out);
    // Strings point into table.
    void toJson(const ThreadTable::Table &table, JsonTable &json);
    // Builds and validates out; nullptr on success, else the reason.
    const char *fromJson(const JsonTable &json, ThreadTable::Table &out);
    // Replaces the live table with a validated one and persists it.
    void replace(const ThreadTable::Table &table);

    // {"groups":[{name,depthExternal,depthInternal,taper}],
    //  "entries":[{group,designation,majorDu,pitchDu}],
    //  "overrides":[{group,designation,depthExternalDu,depthInternalDu,hasTaper,taper}]}.
    // -1 if it doesn't fit.
    int toJson(char *out, size_t outMax);

    // Replaces the table with the same JSON shape, overrides optional (json
    // is modified in place). nullptr on success, else the reason.
    const char *fromJson(char *json, size_t len);
}

#endif
