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
#ifndef ThreadTable_h
#define ThreadTable_h

#include <stddef.h>
#include <stdint.h>

// Thread presets grouped by standard, and the built-in defaults. No Zephyr
// dependency, host testable.
namespace ThreadTable
{
    constexpr size_t GROUP_NAME_MAX = 12;
    constexpr size_t DESIGNATION_MAX = 16;
    // What the display's thread picker shows: tabs, and buttons per tab.
    constexpr size_t MAX_GROUPS = 9;
    constexpr size_t MAX_GROUP_ENTRIES = 35;
    constexpr size_t MAX_ENTRIES = MAX_GROUPS * MAX_GROUP_ENTRIES;
    constexpr float MAX_FLANK_ANGLE = 120;

    // A standard: thread form as depth factors (single-side depth per
    // pitch) and included flank angle, plus taper (diameter change per
    // length, toward the large end).
    struct Group
    {
        char name[GROUP_NAME_MAX];
        float depthExternal;
        float depthInternal;
        float taper;
        float flankAngle; // degrees
    };

    // Depth and taper overrides replace the group's for this entry.
    struct Entry
    {
        uint8_t group;
        bool hasTaper;
        char designation[DESIGNATION_MAX]; // e.g. "M10x1.25", "1/4-20UNC", "G1/2", "1/2NPT"
        int32_t majorDu;
        int32_t pitchDu;
        int32_t depthExternalDu; // 0 = group's
        int32_t depthInternalDu; // 0 = group's
        float taper;
    };

    struct Table
    {
        Group groups[MAX_GROUPS];
        size_t groupCount;
        Entry entries[MAX_ENTRIES];
        size_t entryCount;
    };

    // Returns how many built-in entries were left out (name too long).
    size_t loadDefaults(Table &table);

    // -1 when absent.
    int findGroup(const Table &table, const char *name);
    const Entry *find(const Table &table, const char *group, const char *designation);

    int32_t depthDu(const Group &group, int32_t pitchDu, bool internal);
    int32_t depthDu(const Table &table, const Entry &entry, bool internal);
    float taper(const Table &table, const Entry &entry);
    float flankAngle(const Table &table, const Entry &entry);

    // nullptr if usable, else the reason.
    const char *validate(const Table &table);
}

#endif
