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
#include <stdio.h>
#include <string.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/settings/settings.h>
#include "PersistBlob.hpp"

LOG_MODULE_REGISTER(persist, LOG_LEVEL_INF);

#ifdef CONFIG_ESP_SPIRAM
#define EXT_RAM_BSS __attribute__((section(".ext_ram.bss")))
#else
#define EXT_RAM_BSS
#endif

namespace
{
    constexpr size_t PATH_MAX_LEN = 48;
    constexpr size_t MAX_SLOTS = 256;
    constexpr size_t ARENA_SIZE = 64 * 1024;

    struct Slot
    {
        char path[PATH_MAX_LEN];
        uint32_t off;
        uint32_t len;
        uint32_t cap;
    };

    Slot g_slots[MAX_SLOTS] EXT_RAM_BSS;
    uint8_t g_arena[ARENA_SIZE] EXT_RAM_BSS;
    size_t g_slotCount = 0;
    size_t g_arenaUsed = 0;
    // True while the cache holds every stored key; otherwise loads go to flash.
    bool g_cached = false;
    K_MUTEX_DEFINE(g_lock);

    struct LoadCtx
    {
        void *buf;
        size_t bufMax;
        size_t n;
    };

    int loadDirect(const char *key, size_t len, settings_read_cb readCb, void *cbArg, void *param)
    {
        LoadCtx *ctx = (LoadCtx *)param;
        size_t toRead = len < ctx->bufMax ? len : ctx->bufMax;
        ssize_t rc = readCb(cbArg, ctx->buf, toRead);
        ctx->n = rc > 0 ? (size_t)rc : 0;
        return 0;
    }

    void makePath(char *out, const char *ns, const char *key)
    {
        snprintf(out, PATH_MAX_LEN, "%s/%s", ns, key);
    }

    Slot *find(const char *path)
    {
        for (size_t i = 0; i < g_slotCount; i++) {
            if (!strcmp(g_slots[i].path, path)) {
                return &g_slots[i];
            }
        }
        return nullptr;
    }

    // Slot with room for len bytes, or nullptr when the cache is full.
    Slot *reserve(const char *path, size_t len)
    {
        Slot *s = find(path);
        if (!s) {
            if (g_slotCount == MAX_SLOTS) {
                return nullptr;
            }
            s = &g_slots[g_slotCount++];
            strcpy(s->path, path);
            s->off = 0;
            s->len = 0;
            s->cap = 0;
        }
        if (len > s->cap) {
            if (g_arenaUsed + len > ARENA_SIZE) {
                return nullptr;
            }
            s->off = g_arenaUsed;
            s->cap = len;
            g_arenaUsed += len;
        }
        return s;
    }

    int preloadOne(const char *name, size_t len, settings_read_cb readCb, void *cbArg, void *)
    {
        if (strlen(name) >= PATH_MAX_LEN) {
            return 0;
        }
        Slot *s = reserve(name, len);
        if (!s) {
            g_cached = false;
            return 0;
        }
        ssize_t rc = readCb(cbArg, g_arena + s->off, len);
        s->len = rc > 0 ? (uint32_t)rc : 0;
        return 0;
    }
}

namespace PersistBlob
{
    void preload()
    {
        k_mutex_lock(&g_lock, K_FOREVER);
        settings_subsys_init();
        g_cached = true;
        settings_load_subtree_direct(nullptr, preloadOne, nullptr);
        if (!g_cached) {
            LOG_WRN("cache full, loading from flash");
        }
        k_mutex_unlock(&g_lock);
    }

    size_t load(const char *ns, const char *key, void *buf, size_t bufMax)
    {
        char path[PATH_MAX_LEN];
        makePath(path, ns, key);
        k_mutex_lock(&g_lock, K_FOREVER);
        size_t n = 0;
        if (g_cached) {
            Slot *s = find(path);
            if (s) {
                n = s->len < bufMax ? s->len : bufMax;
                memcpy(buf, g_arena + s->off, n);
            }
        } else {
            LoadCtx ctx{buf, bufMax, 0};
            settings_load_subtree_direct(path, loadDirect, &ctx);
            n = ctx.n;
        }
        k_mutex_unlock(&g_lock);
        return n;
    }

    bool save(const char *ns, const char *key, const void *buf, size_t len)
    {
        char path[PATH_MAX_LEN];
        makePath(path, ns, key);
        k_mutex_lock(&g_lock, K_FOREVER);
        bool ok = settings_save_one(path, buf, len) == 0;
        if (ok && g_cached) {
            Slot *s = reserve(path, len);
            if (s) {
                memcpy(g_arena + s->off, buf, len);
                s->len = len;
            } else {
                g_cached = false;
                LOG_WRN("cache full, loading from flash");
            }
        }
        k_mutex_unlock(&g_lock);
        return ok;
    }
}
