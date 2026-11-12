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
#ifndef GcodeCore_h
#define GcodeCore_h

#include <stddef.h>
#include <stdint.h>

// Flavor-neutral G-code pieces: tokenizer, interpreted Step, Flavor
// interface. No Zephyr dependency; no heap allocation.
namespace Gcode
{
    constexpr int32_t DU_PER_MM = 10000;
    constexpr int32_t DU_PER_INCH = 254000;

    constexpr size_t MAX_WORDS = 16;
    constexpr size_t MAX_LINE_LEN = 128;

    struct Word
    {
        char letter;
        double value;
    };

    struct Words
    {
        Word words[MAX_WORDS];
        size_t count;
    };

    // Splits one source line into words. Strips ';' and '(...)' comments,
    // drops N<num> words and lines that are just "%". nullptr on success,
    // else a static error string.
    const char *tokenize(const char *text, Words &out);

    enum class StepKind : uint8_t
    {
        Rapid,
        Feed,
        Thread,
        Pause,
        OptionalPause,
        End
    };

    // One interpreted motion/control action. zDu/xDu are absolute targets,
    // meaningful for the motion kinds only.
    struct Step
    {
        StepKind kind;
        int32_t zDu;
        int32_t xDu;
        int32_t feed; // du/min for Feed, lead du per spindle rev for Thread
        uint32_t line;
    };

    struct StepSink
    {
        virtual void emit(const Step &step) = 0;
        virtual ~StepSink()
        {}
    };

    struct Flavor
    {
        virtual void begin(int32_t startZDu, int32_t startXDu) = 0;
        virtual const char *line(const Words &words, uint32_t lineNo, StepSink &sink) = 0;
        virtual const char *id() const = 0;
        virtual ~Flavor()
        {}
    };

    const char *interpretLine(Flavor &flavor, const char *text, uint32_t lineNo, StepSink &sink);
}

#endif
