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

// Dialect-neutral G-code pieces: words, interpreted Step, Dialect
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

    enum class StepKind : uint8_t
    {
        Rapid,
        Feed,
        Thread,
        Pause,
        OptionalPause,
        End,
        SetPosition,  // G10 L20: the axes in mask read zDu/xDu
        SpindleAngle, // M110: the current angle becomes feed millidegrees
        Probe,        // G38.x: one axis toward zDu/xDu at feed, see the probe flags
    };

    constexpr uint8_t PROBE_AWAY = 1;     // G38.4/.5: stops when the contact opens
    constexpr uint8_t PROBE_NO_ERROR = 2; // G38.3/.5: no error if the input does not change

    constexpr uint8_t AXIS_Z = 1;
    constexpr uint8_t AXIS_X = 2;

    // One interpreted motion/control action. For the motion kinds zDu/xDu
    // are absolute targets.
    struct Step
    {
        StepKind kind;
        int32_t zDu;
        int32_t xDu;
        int32_t feed; // du/min for Feed, lead du per spindle rev for Thread
        uint32_t line;
        uint8_t axes;
        uint8_t probe;
    };

    struct StepSink
    {
        virtual void emit(const Step &step) = 0;
        virtual ~StepSink()
        {}
    };

    class Interpreter;

    // Turns one dialect's lines into the standard (LinuxCNC) words the
    // Interpreter runs.
    struct Dialect
    {
        virtual const char *id() const = 0;
        // Resets interp and the dialect's own state for a new program.
        virtual void begin(Interpreter &interp, int32_t startZDu, int32_t startXDu) = 0;
        virtual const char *translate(Words &words) = 0;
        virtual ~Dialect()
        {}
    };
}

#endif
