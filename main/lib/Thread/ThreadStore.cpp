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
#include "PersistBlob.hpp"
#include <stdio.h>
#include <string.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/util.h>

LOG_MODULE_REGISTER(threads, LOG_LEVEL_INF);

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

    constexpr const char *NVS_NS = "threads";
    // Keeps each blob well inside one NVS sector.
    constexpr size_t ENTRIES_PER_CHUNK = 64;

    K_MUTEX_DEFINE(g_lock);
    Table g_table EXT_RAM_BSS;

    struct StoredGroups
    {
        uint32_t count;
        Group groups[ThreadTable::MAX_GROUPS];
    };

    void chunkKey(size_t chunk, char *out, size_t outMax)
    {
        snprintf(out, outMax, "e%u", (unsigned)chunk);
    }

    void loadBuiltIn(Table &t)
    {
        size_t skipped = ThreadTable::loadDefaults(t);
        if (skipped) {
            LOG_WRN("%u built-in threads left out: designation too long", (unsigned)skipped);
        }
    }

    bool loadStored(Table &t)
    {
        uint8_t custom = 0;
        if (PersistBlob::load(NVS_NS, "custom", &custom, sizeof(custom)) != sizeof(custom) || !custom) {
            return false;
        }
        StoredGroups g;
        uint32_t entryCount;
        if (PersistBlob::load(NVS_NS, "groups", &g, sizeof(g)) != sizeof(g) ||
            PersistBlob::load(NVS_NS, "count", &entryCount, sizeof(entryCount)) != sizeof(entryCount) ||
            g.count > ThreadTable::MAX_GROUPS || entryCount > ThreadTable::MAX_ENTRIES) {
            return false;
        }
        memcpy(t.groups, g.groups, sizeof(g.groups));
        t.groupCount = g.count;
        t.entryCount = entryCount;
        for (size_t first = 0; first < entryCount; first += ENTRIES_PER_CHUNK) {
            size_t n = MIN(ENTRIES_PER_CHUNK, entryCount - first);
            char key[12];
            chunkKey(first / ENTRIES_PER_CHUNK, key, sizeof(key));
            if (PersistBlob::load(NVS_NS, key, &t.entries[first], n * sizeof(Entry)) != n * sizeof(Entry)) {
                return false;
            }
        }
        return ThreadTable::validate(t) == nullptr;
    }

    void saveStored(const Table &t)
    {
        uint8_t custom = 0;
        PersistBlob::save(NVS_NS, "custom", &custom, sizeof(custom));
        StoredGroups g = {};
        g.count = t.groupCount;
        memcpy(g.groups, t.groups, sizeof(Group) * t.groupCount);
        uint32_t entryCount = t.entryCount;
        PersistBlob::save(NVS_NS, "groups", &g, sizeof(g));
        PersistBlob::save(NVS_NS, "count", &entryCount, sizeof(entryCount));
        for (size_t first = 0; first < t.entryCount; first += ENTRIES_PER_CHUNK) {
            size_t n = MIN(ENTRIES_PER_CHUNK, t.entryCount - first);
            char key[12];
            chunkKey(first / ENTRIES_PER_CHUNK, key, sizeof(key));
            PersistBlob::save(NVS_NS, key, &t.entries[first], n * sizeof(Entry));
        }
        custom = 1;
        PersistBlob::save(NVS_NS, "custom", &custom, sizeof(custom));
    }
}

namespace ThreadStore
{
    void begin()
    {
        k_mutex_lock(&g_lock, K_FOREVER);
        if (!loadStored(g_table)) {
            loadBuiltIn(g_table);
        }
        k_mutex_unlock(&g_lock);
    }

    void reset()
    {
        k_mutex_lock(&g_lock, K_FOREVER);
        uint8_t custom = 0;
        PersistBlob::save(NVS_NS, "custom", &custom, sizeof(custom));
        loadBuiltIn(g_table);
        k_mutex_unlock(&g_lock);
    }

    bool lookup(const char *group, const char *designation, Preset &out)
    {
        k_mutex_lock(&g_lock, K_FOREVER);
        const Entry *e = ThreadTable::find(g_table, group, designation);
        if (e) {
            out = {e->pitchDu, e->majorDu, ThreadTable::depthDu(g_table, *e, false),
                   ThreadTable::depthDu(g_table, *e, true), ThreadTable::taper(g_table, *e),
                   ThreadTable::flankAngle(g_table, *e)};
        }
        k_mutex_unlock(&g_lock);
        return e != nullptr;
    }

    void groups(char *out, size_t outMax)
    {
        k_mutex_lock(&g_lock, K_FOREVER);
        size_t n = 0;
        out[0] = '\0';
        for (size_t i = 0; i < g_table.groupCount && n < outMax; i++) {
            n += snprintf(out + n, outMax - n, i ? ",%s" : "%s", g_table.groups[i].name);
        }
        k_mutex_unlock(&g_lock);
    }

    bool list(const char *group, size_t offset, char *out, size_t outMax, size_t &total, size_t &next)
    {
        k_mutex_lock(&g_lock, K_FOREVER);
        int g = ThreadTable::findGroup(g_table, group);
        if (g < 0) {
            k_mutex_unlock(&g_lock);
            return false;
        }
        total = 0;
        next = 0;
        size_t n = 0;
        bool full = false;
        out[0] = '\0';
        for (size_t i = 0; i < g_table.entryCount; i++) {
            const Entry &e = g_table.entries[i];
            if (e.group != g) {
                continue;
            }
            if (total >= offset && !full) {
                size_t len = strlen(e.designation) + (n ? 1 : 0);
                if (n + len >= outMax) {
                    full = true;
                } else {
                    n += snprintf(out + n, outMax - n, n ? ",%s" : "%s", e.designation);
                    next = total + 1;
                }
            }
            total++;
        }
        if (!full) {
            next = total;
        }
        k_mutex_unlock(&g_lock);
        return true;
    }

    void snapshot(Table &out)
    {
        k_mutex_lock(&g_lock, K_FOREVER);
        memcpy(&out, &g_table, sizeof(out));
        k_mutex_unlock(&g_lock);
    }

    void replace(const Table &table)
    {
        k_mutex_lock(&g_lock, K_FOREVER);
        memcpy(&g_table, &table, sizeof(g_table));
        saveStored(g_table);
        k_mutex_unlock(&g_lock);
    }
}
