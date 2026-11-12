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
#ifndef GcodeMeta_h
#define GcodeMeta_h

#include "GcodeCore.hpp"
#include <stddef.h>
#include <stdint.h>

// Pure helpers for saved-program metadata: name validation, the one-line
// file header, and dry-run validation against a flavor. No Zephyr
// dependency, so these are unit-testable on native_sim.
namespace GcodeMeta
{
    constexpr size_t NAME_MAX_LEN = 63;

    // 1..NAME_MAX_LEN UTF-8 bytes, no ASCII control chars.
    bool validName(const char *name, size_t len);

    // Writes "<flavor>\t<name>\n" to out. Returns the length, or -1 if it
    // doesn't fit outMax.
    int encodeHeader(const char *flavor, const char *name, char *out, size_t outMax);

    // Points into text; nothing is copied.
    struct Header
    {
        const char *flavor;
        size_t flavorLen;
        const char *name;
        size_t nameLen;
        size_t bodyOffset;
    };

    // Decodes the first line of text as "<flavor>\t<name>\n". False if
    // there's no header line or it has no tab-separated flavor and name.
    // With allowEmpty, an empty flavor or name is accepted.
    bool decodeHeader(const char *text, size_t len, Header &out, bool allowEmpty = false);

    struct ValidateResult
    {
        bool ok;
        const char *error;
        uint32_t line; // 1-based, 0 if the error isn't tied to one line
        uint32_t steps; // motion steps emitted, valid only if ok
    };

    // Dry-runs text through flavor: begin(0, 0), then interpretLine per
    // line with a counting sink. Rejects an empty program, an embedded NUL
    // byte, an axesMismatch() header, a line at or over Gcode::MAX_LINE_LEN, a flavor syntax error
    // (first one wins), and a program with zero motion steps.
    ValidateResult validateProgram(const char *text, size_t len, Gcode::Flavor &flavor);

    // True if text carries the LatheCode generator's
    // "; Axes expected: Z+ points <left|right>, X+ points <up|down>"
    // header and it isn't "right, down" -- the only mapping that matches
    // our axes (-Z toward the chuck, -X toward the centerline). False
    // (no mismatch) when there's no such header.
    bool axesMismatch(const char *text, size_t len);
}

#endif
