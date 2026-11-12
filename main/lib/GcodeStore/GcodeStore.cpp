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
#include "GcodeStore.hpp"
#include "GcodeFiles.hpp"
#include "GcodeFlavors.hpp"
#include "Mode.hpp"
#include <string.h>
#include <zephyr/kernel.h>

#ifdef CONFIG_ESP_SPIRAM
#define EXT_RAM_BSS __attribute__((section(".ext_ram.bss")))
#else
#define EXT_RAM_BSS
#endif

namespace
{
    K_MUTEX_DEFINE(g_lock);

    struct Guard
    {
        Guard()
        {
            k_mutex_lock(&g_lock, K_FOREVER);
        }
        ~Guard()
        {
            k_mutex_unlock(&g_lock);
        }
    };

    // Saved programs are read here first, so a corrupt one can't clobber the active buffer.
    uint8_t g_stageBuf[GcodeStore::PROGRAM_MAX] EXT_RAM_BSS;

    uint8_t g_activeBuf[GcodeStore::PROGRAM_MAX] EXT_RAM_BSS;
    GcodeStore::ActiveInfo g_active;

    bool blockedRunning()
    {
        return Mode::current() == Mode::Id::Gcode && Mode::running();
    }

    bool isActive(const char *id)
    {
        return g_active.present && strcmp(g_active.savedId, id) == 0;
    }

    template <size_t N>
    void copyStr(char (&out)[N], const char *s)
    {
        strncpy(out, s, N - 1);
        out[N - 1] = '\0';
    }

    void setActive(const char *text, size_t len, const char *flavor, const char *name, const char *savedId,
                   uint32_t steps)
    {
        memcpy(g_activeBuf, text, len);
        g_active.present = true;
        g_active.size = len;
        copyStr(g_active.flavor, flavor);
        copyStr(g_active.name, name);
        copyStr(g_active.savedId, savedId ? savedId : "");
        g_active.steps = steps;
        g_active.version++;
    }

    const char *loadSaved(const char *id, GcodeStore::ProgramInfo &info)
    {
        return GcodeFiles::load(id, info, g_stageBuf, sizeof(g_stageBuf));
    }

    GcodeStore::Result fail(const char *error, uint32_t line = 0)
    {
        return GcodeStore::Result{false, error, line, 0, ""};
    }

    GcodeStore::Result success(uint32_t steps, const char *savedId)
    {
        GcodeStore::Result r = {true, nullptr, 0, steps, ""};
        copyStr(r.savedId, savedId);
        return r;
    }

    GcodeStore::Result validate(const char *text, size_t len, const char *flavorId)
    {
        Gcode::Flavor *flavor = Gcode::find(flavorId);
        if (!flavor) {
            return fail("unknown flavor");
        }
        GcodeMeta::ValidateResult v = GcodeMeta::validateProgram(text, len, *flavor);
        return v.ok ? success(v.steps, "") : fail(v.error, v.line);
    }

    bool decode(const char *body, size_t len, bool allowEmpty, GcodeMeta::Header &h,
                char (&flavor)[GcodeStore::FLAVOR_MAX_LEN + 1])
    {
        if (!GcodeMeta::decodeHeader(body, len, h, allowEmpty) || h.flavorLen > GcodeStore::FLAVOR_MAX_LEN) {
            return false;
        }
        memcpy(flavor, h.flavor, h.flavorLen);
        flavor[h.flavorLen] = '\0';
        return true;
    }
}

namespace GcodeStore
{
    bool list(ListResult &out)
    {
        Guard guard;
        return GcodeFiles::list(out);
    }

    Result upload(const char *text, size_t len, const char *name, size_t nameLen, const char *flavorId, bool save,
                  bool activate)
    {
        Guard guard;
        if (!save && !activate) {
            return fail("nothing to do");
        }
        if (activate && blockedRunning()) {
            return fail("gcode job running");
        }
        if (len > PROGRAM_MAX) {
            return fail("program too large");
        }
        if (!GcodeMeta::validName(name, nameLen)) {
            return fail("bad name");
        }
        Result r = validate(text, len, flavorId);
        if (!r.ok) {
            return r;
        }

        char nameBuf[GcodeMeta::NAME_MAX_LEN + 1];
        memcpy(nameBuf, name, nameLen);
        nameBuf[nameLen] = '\0';

        if (save) {
            if (!GcodeFiles::mount()) {
                return fail("no filesystem");
            }
            GcodeFiles::nextId(r.savedId);
            if (!GcodeFiles::save(r.savedId, flavorId, nameBuf, text, len)) {
                return fail("save failed");
            }
        }
        if (activate) {
            setActive(text, len, flavorId, nameBuf, save ? r.savedId : nullptr, r.steps);
        }
        return r;
    }

    Result update(const char *id, const char *name, size_t nameLen, const char *flavorId, const char *text, size_t len)
    {
        Guard guard;
        if (len > PROGRAM_MAX) {
            return fail("program too large");
        }
        ProgramInfo info;
        const char *error = loadSaved(id, info);
        if (error) {
            return fail(error);
        }
        bool active = isActive(id);
        if (active && blockedRunning()) {
            return fail("gcode job running");
        }
        if (nameLen) {
            if (!GcodeMeta::validName(name, nameLen)) {
                return fail("bad name");
            }
            memcpy(info.name, name, nameLen);
            info.name[nameLen] = '\0';
        }
        if (flavorId[0]) {
            if (strlen(flavorId) > FLAVOR_MAX_LEN) {
                return fail("unknown flavor");
            }
            strcpy(info.flavor, flavorId);
        }

        const char *body = len ? text : (const char *)g_stageBuf;
        size_t bodyLen = len ? len : info.size;
        Result v = validate(body, bodyLen, info.flavor);
        if (!v.ok) {
            return v;
        }
        if (!GcodeFiles::save(info.id, info.flavor, info.name, body, bodyLen)) {
            return fail("save failed");
        }
        if (active) {
            setActive(body, bodyLen, info.flavor, info.name, info.id, v.steps);
        }
        return success(v.steps, info.id);
    }

    Result uploadEncoded(const char *body, size_t len, bool save, bool activate)
    {
        GcodeMeta::Header h;
        char flavor[FLAVOR_MAX_LEN + 1];
        if (!decode(body, len, false, h, flavor)) {
            return fail("missing flavor/name line");
        }
        return upload(body + h.bodyOffset, len - h.bodyOffset, h.name, h.nameLen, flavor, save, activate);
    }

    Result updateEncoded(const char *id, const char *body, size_t len)
    {
        GcodeMeta::Header h;
        char flavor[FLAVOR_MAX_LEN + 1];
        if (!decode(body, len, true, h, flavor)) {
            return fail("missing flavor/name line");
        }
        return update(id, h.name, h.nameLen, flavor, body + h.bodyOffset, len - h.bodyOffset);
    }

    Result select(const char *id)
    {
        Guard guard;
        if (blockedRunning()) {
            return fail("gcode job running");
        }
        ProgramInfo info;
        const char *error = loadSaved(id, info);
        if (error) {
            return fail(error);
        }
        Result v = validate((const char *)g_stageBuf, info.size, info.flavor);
        if (!v.ok) {
            return v;
        }
        setActive((const char *)g_stageBuf, info.size, info.flavor, info.name, info.id, v.steps);
        return success(v.steps, info.id);
    }

    const char *read(const char *id, const char *&text, size_t &len)
    {
        Guard guard;
        ProgramInfo info;
        const char *error = loadSaved(id, info);
        if (error) {
            return error;
        }
        text = (const char *)g_stageBuf;
        len = info.size;
        return nullptr;
    }

    const char *remove(const char *id)
    {
        Guard guard;
        if (!GcodeFiles::mount()) {
            return "no filesystem";
        }
        if (!GcodeFiles::validId(id)) {
            return "bad id";
        }
        bool active = isActive(id);
        if (active && blockedRunning()) {
            return "gcode job running";
        }
        const char *error = GcodeFiles::remove(id);
        if (error) {
            return error;
        }
        if (active) {
            g_active.savedId[0] = '\0';
        }
        return nullptr;
    }

    ActiveInfo active()
    {
        Guard guard;
        return g_active;
    }

    const char *activeText(size_t &len)
    {
        Guard guard;
        const char *text = g_active.present ? (const char *)g_activeBuf : nullptr;
        len = g_active.present ? g_active.size : 0;
        return text;
    }
}
