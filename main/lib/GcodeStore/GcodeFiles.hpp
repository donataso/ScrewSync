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
#ifndef GcodeFiles_h
#define GcodeFiles_h

#include "GcodeStore.hpp"
#include <stddef.h>
#include <stdint.h>

// Program files on the "gcode" LittleFS partition, one "<id>.gc" each:
// "<flavor>\t<name>\n" then the text. Callers hold GcodeStore's lock.
namespace GcodeFiles
{
    bool mount();
    bool validId(const char *id);

    // id of a new file: one past the highest saved.
    void nextId(char (&out)[GcodeStore::ID_LEN + 1]);

    // Written to a temp file then renamed, so an interrupted save leaves no partial file.
    bool save(const char *id, const char *flavor, const char *name, const char *text, size_t len);

    // A saved program's metadata and body into body (at most bodyMax). nullptr on success, else the reason.
    const char *load(const char *id, GcodeStore::ProgramInfo &info, uint8_t *body, size_t bodyMax);

    bool list(GcodeStore::ListResult &out);

    // nullptr on success, else the reason.
    const char *remove(const char *id);
}

#endif
