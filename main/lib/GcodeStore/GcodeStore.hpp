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
#ifndef GcodeStore_h
#define GcodeStore_h

#include "GcodeMeta.hpp"
#include <stddef.h>
#include <stdint.h>

// Saved G-code programs on the "gcode" LittleFS partition, plus the one
// active program (PSRAM buffer) that execution and selection both read
// from. The partition is mounted lazily, on first use, never at boot.
namespace GcodeStore
{
    constexpr size_t PROGRAM_MAX = 512 * 1024;
    constexpr size_t FLAVOR_MAX_LEN = 15;
    constexpr size_t ID_LEN = 8; // hex digits, excluding the NUL
    constexpr size_t LIST_MAX = 64;

    struct ProgramInfo
    {
        char id[ID_LEN + 1];
        char name[GcodeMeta::NAME_MAX_LEN + 1];
        char flavor[FLAVOR_MAX_LEN + 1];
        size_t size; // program text, excluding the header line
    };

    struct ListResult
    {
        ProgramInfo programs[LIST_MAX];
        size_t count;
        uint64_t freeBytes;
    };

    struct Result
    {
        bool ok;
        const char *error;
        uint32_t line; // 1-based, 0 if not tied to a line
        uint32_t steps;
        char savedId[ID_LEN + 1]; // empty if not saved
    };

    struct ActiveInfo
    {
        bool present;
        char name[GcodeMeta::NAME_MAX_LEN + 1];
        char flavor[FLAVOR_MAX_LEN + 1];
        size_t size;
        uint32_t steps;
        char savedId[ID_LEN + 1]; // empty if not saved
        uint32_t version;
    };

    // Lists saved programs (mounts the partition on first call) and its
    // free space. False if the partition can't be mounted.
    bool list(ListResult &out);

    // Validates text against flavor, then saves it as a new file and/or
    // makes it active. Activating is refused while a Gcode job is running.
    Result upload(const char *text, size_t len, const char *name, size_t nameLen, const char *flavor, bool save,
                  bool activate);

    // Overwrites a saved program in place. An empty name, empty flavor or
    // zero-length text leaves that field as it was. Refused if it's the
    // active program and a Gcode job is running.
    Result update(const char *id, const char *name, size_t nameLen, const char *flavor, const char *text, size_t len);

    // Encoded body: "<flavor>\t<name>\n" (the saved-file header line), then the program.
    Result uploadEncoded(const char *body, size_t len, bool save, bool activate);
    // An empty flavor, empty name or missing program leaves that field.
    Result updateEncoded(const char *id, const char *body, size_t len);
    // Largest encoded body.
    constexpr size_t ENCODED_MAX = PROGRAM_MAX + GcodeMeta::NAME_MAX_LEN + FLAVOR_MAX_LEN + 2;

    // A saved program's text, without making it active. Valid until the
    // next read or select. nullptr on success, else the reason.
    const char *read(const char *id, const char *&text, size_t &len);

    // Loads a saved program into the active buffer. Refused while a Gcode
    // job is running.
    Result select(const char *id);

    // Deletes a saved program. Refused only if it's the active program and
    // a Gcode job is running; otherwise, if it's the active program, the
    // active buffer stays but its saved id becomes none. nullptr on
    // success, else the reason.
    const char *remove(const char *id);

    ActiveInfo active();

    // The active program's text and length; nullptr/0 if none.
    const char *activeText(size_t &len);
}

#endif
