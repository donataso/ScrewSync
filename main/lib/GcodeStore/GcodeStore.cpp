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
#include "GcodeDialects.hpp"
#include "GcodeProgram.hpp"
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

    constexpr size_t ROUTINE_MAX = 32 * 1024;
    constexpr size_t ROUTINE_SLOTS = 8;
    uint8_t g_routineBuf[ROUTINE_SLOTS][ROUTINE_MAX] EXT_RAM_BSS;
    struct RoutineSlot
    {
        char label[Gcode::LABEL_MAX + 1];
        const char *text;
        size_t len;
    };
    RoutineSlot g_slots[ROUTINE_SLOTS];
    size_t g_slotCount = 0;

    const char PROBE_NGC[] = {
#include "routine_probe.inc"
    };
    const char ZERO_NGC[] = {
#include "routine_zero.inc"
    };
    const char BACKLASH_NGC[] = {
#include "routine_backlash.inc"
    };

    struct BuiltIn
    {
        const char *label;
        const char *text;
        size_t len;
    };
    const BuiltIn BUILT_INS[] = {
        {"probe", PROBE_NGC, sizeof(PROBE_NGC)},
        {"zero", ZERO_NGC, sizeof(ZERO_NGC)},
        {"backlash", BACKLASH_NGC, sizeof(BACKLASH_NGC)},
    };

    bool isRoutine(const char *dialect)
    {
        return !strcmp(dialect, Gcode::ROUTINE_DIALECT);
    }

    // Before the store lock: Mode's lock is taken before the store's, never after.
    bool gcodeRunning()
    {
        return Mode::gcodeRunning();
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

    void setActive(const char *text, size_t len, const char *dialect, const char *name, const char *savedId,
                   uint32_t steps, bool stepsKnown)
    {
        memcpy(g_activeBuf, text, len);
        g_active.present = true;
        g_active.size = len;
        copyStr(g_active.dialect, dialect);
        copyStr(g_active.name, name);
        copyStr(g_active.savedId, savedId ? savedId : "");
        g_active.steps = steps;
        g_active.stepsKnown = stepsKnown;
        g_active.version++;
    }

    const char *loadSaved(const char *id, GcodeStore::ProgramInfo &info)
    {
        return GcodeFiles::load(id, info, g_stageBuf, sizeof(g_stageBuf));
    }

    GcodeStore::Result fail(const char *error, uint32_t line = 0)
    {
        return GcodeStore::Result{false, error, line, 0, false, ""};
    }

    GcodeStore::Result success(uint32_t steps, bool stepsKnown, const char *savedId)
    {
        GcodeStore::Result r = {true, nullptr, 0, steps, stepsKnown, ""};
        copyStr(r.savedId, savedId);
        return r;
    }

    // exceptId: the saved program being replaced, "" if none.
    GcodeStore::Result validate(const char *text, size_t len, const char *dialectId, const char *name,
                                const char *exceptId)
    {
        Gcode::Dialect *dialect = Gcode::find(dialectId);
        if (!dialect) {
            return fail("unknown dialect");
        }
        GcodeMeta::ValidateResult v = GcodeMeta::validateProgram(text, len, *dialect);
        if (!v.ok) {
            return fail(v.error, v.line);
        }
        if (isRoutine(dialectId)) {
            char label[Gcode::LABEL_MAX + 1];
            char other[GcodeStore::ID_LEN + 1];
            size_t pos;
            uint32_t line;
            if (!Gcode::routineLabel(name, strlen(name), label)) {
                return fail("bad routine name");
            }
            if (!Gcode::findSub(text, len, label, pos, line)) {
                return fail("routine needs o<name> sub");
            }
            if (GcodeFiles::findRoutine(label, exceptId, other)) {
                return fail("routine name in use");
            }
        }
        return success(v.steps, v.stepsKnown, "");
    }

    bool decode(const char *body, size_t len, bool allowEmpty, GcodeMeta::Header &h,
                char (&dialect)[GcodeStore::DIALECT_MAX_LEN + 1])
    {
        if (!GcodeMeta::decodeHeader(body, len, h, allowEmpty) || h.dialectLen > GcodeStore::DIALECT_MAX_LEN) {
            return false;
        }
        memcpy(dialect, h.dialect, h.dialectLen);
        dialect[h.dialectLen] = '\0';
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

    Result upload(const char *text, size_t len, const char *name, size_t nameLen, const char *dialectId, bool save,
                  bool activate)
    {
        bool running = gcodeRunning();
        Guard guard;
        if (!save && !activate) {
            return fail("nothing to do");
        }
        if (activate && isRoutine(dialectId)) {
            return fail("a routine is called, not run");
        }
        if (activate && running) {
            return fail("gcode job running");
        }
        if (len > PROGRAM_MAX) {
            return fail("program too large");
        }
        if (!GcodeMeta::validName(name, nameLen)) {
            return fail("bad name");
        }
        char nameBuf[GcodeMeta::NAME_MAX_LEN + 1];
        memcpy(nameBuf, name, nameLen);
        nameBuf[nameLen] = '\0';
        Result r = validate(text, len, dialectId, nameBuf, "");
        if (!r.ok) {
            return r;
        }

        if (save) {
            if (!GcodeFiles::mount()) {
                return fail("no filesystem");
            }
            GcodeFiles::nextId(r.savedId);
            if (!GcodeFiles::save(r.savedId, dialectId, nameBuf, text, len)) {
                return fail("save failed");
            }
        }
        if (activate) {
            setActive(text, len, dialectId, nameBuf, save ? r.savedId : nullptr, r.steps, r.stepsKnown);
        }
        return r;
    }

    Result update(const char *id, const char *name, size_t nameLen, const char *dialectId, const char *text, size_t len)
    {
        bool running = gcodeRunning();
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
        if ((active || isRoutine(info.dialect)) && running) {
            return fail("gcode job running");
        }
        if (nameLen) {
            if (!GcodeMeta::validName(name, nameLen)) {
                return fail("bad name");
            }
            memcpy(info.name, name, nameLen);
            info.name[nameLen] = '\0';
        }
        if (dialectId[0]) {
            if (strlen(dialectId) > DIALECT_MAX_LEN) {
                return fail("unknown dialect");
            }
            strcpy(info.dialect, dialectId);
        }
        if (active && isRoutine(info.dialect)) {
            return fail("a routine is called, not run");
        }

        const char *body = len ? text : (const char *)g_stageBuf;
        size_t bodyLen = len ? len : info.size;
        Result v = validate(body, bodyLen, info.dialect, info.name, info.id);
        if (!v.ok) {
            return v;
        }
        if (!GcodeFiles::save(info.id, info.dialect, info.name, body, bodyLen)) {
            return fail("save failed");
        }
        if (active) {
            setActive(body, bodyLen, info.dialect, info.name, info.id, v.steps, v.stepsKnown);
        }
        return success(v.steps, v.stepsKnown, info.id);
    }

    Result uploadEncoded(const char *body, size_t len, bool save, bool activate)
    {
        GcodeMeta::Header h;
        char dialect[DIALECT_MAX_LEN + 1];
        if (!decode(body, len, false, h, dialect)) {
            return fail("missing dialect/name line");
        }
        return upload(body + h.bodyOffset, len - h.bodyOffset, h.name, h.nameLen, dialect, save, activate);
    }

    Result updateEncoded(const char *id, const char *body, size_t len)
    {
        GcodeMeta::Header h;
        char dialect[DIALECT_MAX_LEN + 1];
        if (!decode(body, len, true, h, dialect)) {
            return fail("missing dialect/name line");
        }
        return update(id, h.name, h.nameLen, dialect, body + h.bodyOffset, len - h.bodyOffset);
    }

    Result select(const char *id)
    {
        bool running = gcodeRunning();
        Guard guard;
        if (running) {
            return fail("gcode job running");
        }
        ProgramInfo info;
        const char *error = loadSaved(id, info);
        if (error) {
            return fail(error);
        }
        if (isRoutine(info.dialect)) {
            return fail("a routine is called, not run");
        }
        Result v = validate((const char *)g_stageBuf, info.size, info.dialect, info.name, info.id);
        if (!v.ok) {
            return v;
        }
        setActive((const char *)g_stageBuf, info.size, info.dialect, info.name, info.id, v.steps, v.stepsKnown);
        return success(v.steps, v.stepsKnown, info.id);
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
        bool running = gcodeRunning();
        Guard guard;
        if (!GcodeFiles::mount()) {
            return "no filesystem";
        }
        if (!GcodeFiles::validId(id)) {
            return "bad id";
        }
        bool active = isActive(id);
        ProgramInfo info;
        bool routine = !loadSaved(id, info) && isRoutine(info.dialect);
        if ((active || routine) && running) {
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

    const char *routine(const char *label, size_t &len)
    {
        Guard guard;
        for (size_t i = 0; i < g_slotCount; i++) {
            if (!strcmp(g_slots[i].label, label)) {
                len = g_slots[i].len;
                return g_slots[i].text;
            }
        }
        if (g_slotCount >= ROUTINE_SLOTS) {
            return nullptr;
        }
        RoutineSlot &slot = g_slots[g_slotCount];
        slot.text = nullptr;
        char id[ID_LEN + 1];
        ProgramInfo info;
        if (GcodeFiles::findRoutine(label, "", id)) {
            if (GcodeFiles::load(id, info, g_routineBuf[g_slotCount], ROUTINE_MAX)) {
                return nullptr;
            }
            slot.text = (const char *)g_routineBuf[g_slotCount];
            slot.len = info.size;
        } else {
            for (const BuiltIn &b : BUILT_INS) {
                if (!strcmp(b.label, label)) {
                    slot.text = b.text;
                    slot.len = b.len;
                }
            }
        }
        if (!slot.text) {
            return nullptr;
        }
        copyStr(slot.label, label);
        g_slotCount++;
        len = slot.len;
        return slot.text;
    }

    void resetRoutines()
    {
        Guard guard;
        g_slotCount = 0;
    }
}
