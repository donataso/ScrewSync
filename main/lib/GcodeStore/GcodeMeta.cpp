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
#include "GcodeMeta.hpp"
#include "GcodeDialects.hpp"
#include "GcodeProgram.hpp"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

namespace
{
    constexpr uint32_t DRY_RUN_LINES_MAX = 200000;

    struct CountingSink : Gcode::StepSink
    {
        uint32_t motionSteps = 0;
        bool probe = false;

        void emit(const Gcode::Step &step) override
        {
            probe = probe || step.kind == Gcode::StepKind::Probe;
            if (step.kind == Gcode::StepKind::Rapid || step.kind == Gcode::StepKind::Feed ||
                step.kind == Gcode::StepKind::Thread) {
                motionSteps++;
            }
        }
    };

    // Case-insensitive substring search in text[from, to). SIZE_MAX if absent.
    size_t findCI(const char *text, size_t from, size_t to, const char *needle)
    {
        size_t needleLen = strlen(needle);
        if (needleLen == 0 || to < needleLen) {
            return (size_t)-1;
        }
        for (size_t i = from; i + needleLen <= to; i++) {
            bool match = true;
            for (size_t k = 0; k < needleLen; k++) {
                char c = text[i + k];
                char lower = (c >= 'A' && c <= 'Z') ? (char)(c - 'A' + 'a') : c;
                if (lower != needle[k]) {
                    match = false;
                    break;
                }
            }
            if (match) {
                return i;
            }
        }
        return (size_t)-1;
    }
}

namespace GcodeMeta
{
    bool validName(const char *name, size_t len)
    {
        if (!name || len == 0 || len > NAME_MAX_LEN) {
            return false;
        }
        for (size_t i = 0; i < len; i++) {
            unsigned char c = (unsigned char)name[i];
            if (c < 0x20 || c == 0x7F) {
                return false;
            }
        }
        return true;
    }

    int encodeHeader(const char *dialect, const char *name, char *out, size_t outMax)
    {
        int n = snprintf(out, outMax, "%s\t%s\n", dialect, name);
        if (n < 0 || (size_t)n >= outMax) {
            return -1;
        }
        return n;
    }

    bool decodeHeader(const char *text, size_t len, Header &out, bool allowEmpty)
    {
        size_t i = 0;
        while (i < len && text[i] != '\n') {
            i++;
        }
        if (i >= len) {
            return false;
        }
        size_t lineLen = i;
        if (lineLen > 0 && text[lineLen - 1] == '\r') {
            lineLen--;
        }

        size_t tab = 0;
        bool found = false;
        for (size_t k = 0; k < lineLen; k++) {
            if (text[k] == '\t') {
                tab = k;
                found = true;
                break;
            }
        }
        if (!found) {
            return false;
        }

        out.dialect = text;
        out.dialectLen = tab;
        out.name = text + tab + 1;
        out.nameLen = lineLen - tab - 1;
        out.bodyOffset = i + 1;
        return allowEmpty || (out.dialectLen > 0 && out.nameLen > 0);
    }

    ValidateResult validateProgram(const char *text, size_t len, Gcode::Dialect &dialect)
    {
        ValidateResult r = {false, nullptr, 0, 0, false};
        if (len == 0) {
            r.error = "empty program";
            return r;
        }
        for (size_t i = 0; i < len; i++) {
            if (text[i] == '\0') {
                r.error = "embedded NUL byte";
                return r;
            }
        }
        if (axesMismatch(text, len)) {
            r.error = "axes mismatch: set AXES RIGHT DOWN in LatheCode and regenerate";
            return r;
        }
        r.error = Gcode::checkProgram(text, len, r.line);
        if (r.error) {
            return r;
        }
        r.line = 0;
        if (!strcmp(dialect.id(), Gcode::ROUTINE_DIALECT)) {
            r.ok = true;
            return r;
        }

        static Gcode::Params params;
        static Gcode::Program program;
        params.clear();
        program.begin({&dialect, &params, nullptr, nullptr, true}, text, len, 0, 0);
        CountingSink sink;
        bool known = true;
        for (uint32_t lines = 0;; lines++) {
            if (lines >= DRY_RUN_LINES_MAX) {
                known = false;
                break;
            }
            Gcode::Program::Status s = program.next(sink, 1);
            if (s == Gcode::Program::Status::Error) {
                r.error = program.error();
                r.line = program.errorLine();
                return r;
            }
            if (sink.probe) {
                known = false;
                break;
            }
            if (s == Gcode::Program::Status::End) {
                known = !program.unknown();
                break;
            }
        }
        if (known && sink.motionSteps == 0) {
            r.error = "no motion steps";
            return r;
        }
        r.ok = true;
        r.steps = sink.motionSteps;
        r.stepsKnown = known;
        return r;
    }

    bool axesMismatch(const char *text, size_t len)
    {
        size_t header = findCI(text, 0, len, "axes expected:");
        if (header == (size_t)-1) {
            return false;
        }
        size_t lineEnd = header;
        while (lineEnd < len && text[lineEnd] != '\n') {
            lineEnd++;
        }

        size_t zPos = findCI(text, header, lineEnd, "z+");
        size_t xPos = findCI(text, header, lineEnd, "x+");
        if (zPos == (size_t)-1 || xPos == (size_t)-1) {
            return false;
        }

        size_t zEnd = xPos > zPos ? xPos : lineEnd;
        bool zRight = findCI(text, zPos, zEnd, "right") != (size_t)-1;
        bool xDown = findCI(text, xPos, lineEnd, "down") != (size_t)-1;
        return !(zRight && xDown);
    }

    bool validId(const char *id, size_t idLen)
    {
        if (strlen(id) != idLen) {
            return false;
        }
        for (size_t i = 0; i < idLen; i++) {
            char ch = id[i];
            if (!((ch >= '0' && ch <= '9') || (ch >= 'a' && ch <= 'f'))) {
                return false;
            }
        }
        return true;
    }

    bool programFileId(const char *name, size_t idLen, uint32_t &id)
    {
        static const char EXT[] = ".gc";
        if (strlen(name) != idLen + sizeof(EXT) - 1 || strcmp(name + idLen, EXT) != 0) {
            return false;
        }
        char hex[17];
        if (idLen >= sizeof(hex)) {
            return false;
        }
        memcpy(hex, name, idLen);
        hex[idLen] = '\0';
        if (!validId(hex, idLen)) {
            return false;
        }
        id = (uint32_t)strtoul(hex, nullptr, 16);
        return true;
    }
}
