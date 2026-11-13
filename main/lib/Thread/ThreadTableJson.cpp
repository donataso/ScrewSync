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

#include <string.h>

namespace
{
    using ThreadTable::Entry;
    using ThreadTable::Group;
    using ThreadTable::Table;

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
    void toJson(const Table &table, JsonTable &json)
    {
        json.groupCount = table.groupCount;
        for (size_t i = 0; i < table.groupCount; i++) {
            const Group &g = table.groups[i];
            json.groups[i] = {g.name, g.depthExternal, g.depthInternal, g.taper, g.flankAngle};
        }
        json.entryCount = table.entryCount;
        for (size_t i = 0; i < table.entryCount; i++) {
            const Entry &e = table.entries[i];
            json.entries[i] = {table.groups[e.group].name, e.designation, e.majorDu, e.pitchDu};
        }
        json.overrideCount = 0;
        for (size_t i = 0; i < table.entryCount; i++) {
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
        for (size_t i = 0; i < json.groupCount; i++) {
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
        for (size_t i = 0; i < json.entryCount; i++) {
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
        for (size_t i = 0; i < json.overrideCount; i++) {
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
}
