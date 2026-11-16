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
#ifndef GcodeNgc_h
#define GcodeNgc_h

#include "GcodeCore.hpp"
#include <stddef.h>
#include <stdint.h>

// LinuxCNC NGC line syntax: parameters, [...] expressions, O-words and
// (MSG,...)/(ABORT,...) comments. No Zephyr dependency; no heap allocation.
namespace Gcode
{
    constexpr size_t LABEL_MAX = 31;
    constexpr size_t MESSAGE_MAX = 63;
    constexpr size_t LOCAL_COUNT = 30;
    constexpr size_t CALL_DEPTH = 8;
    constexpr size_t NAMED_MAX = 64;
    constexpr size_t ASSIGN_MAX = 8;
    constexpr int32_t PROBE_FIRST = 5061;
    constexpr int32_t PROBE_LAST = 5070;

    // Predefined read-only "_" parameters (settings, live inputs).
    struct ParamSource
    {
        // length: value is in du and reads in program units. live: it can
        // change while a program runs.
        virtual bool get(const char *name, double &value, bool &length, bool &live) = 0;
        virtual ~ParamSource()
        {}
    };

    // #1-#30 per call level, named #<x> per call level, named #<_x> global,
    // #5061-#5070 probe results.
    class Params
    {
    public:
        void clear();
        // Back to the main level; its values stay.
        void resetCalls();
        bool enter(const double *args, size_t count);
        void leave();

        // nullptr on success, else the reason.
        const char *numbered(int32_t n, double &value) const;
        const char *setNumbered(int32_t n, double value);
        bool named(const char *name, double &value) const;
        const char *setNamed(const char *name, double value);
        // Program units, work coordinates; motor: the probed axis's motor
        // position (#<_probe_motor>), without backlash compensation.
        void setProbe(double zValue, double xValue, bool found, double motor);

    private:
        struct Named
        {
            char name[LABEL_MAX + 1];
            double value;
            uint8_t level;
        };
        Named *find(const char *name, uint8_t level);
        const Named *find(const char *name, uint8_t level) const;
        uint8_t levelOf(const char *name) const;

        double locals_[CALL_DEPTH + 1][LOCAL_COUNT];
        Named named_[NAMED_MAX];
        size_t namedCount_ = 0;
        double probe_[PROBE_LAST - PROBE_FIRST + 1];
        double probeMotor_ = 0;
        uint8_t depth_ = 0;
    };

    struct Context
    {
        Params *params;
        ParamSource *source;
        int32_t unitScaleDu;
        bool check = false;   // syntax only: reads give 0, math errors are ignored
        bool dryRun = false;  // a probe result, live or undefined value sets unknown
        bool unknown = false;
    };

    enum class OKind : uint8_t
    {
        None,
        Sub,
        Endsub,
        Call,
        If,
        Else,
        Endif,
        While,
        Endwhile,
        Repeat,
        Endrepeat,
    };

    enum class Comment : uint8_t
    {
        None,
        Msg,
        Abort,
        Print,
    };

    struct Assign
    {
        int32_t number; // 0 for a named parameter
        char name[LABEL_MAX + 1];
        double value;
    };

    struct Line
    {
        Words words;
        Assign assigns[ASSIGN_MAX];
        size_t assignCount;
        OKind okind;
        char label[LABEL_MAX + 1];
        double args[LOCAL_COUNT]; // call arguments; if/while/repeat: args[0]
        size_t argCount;
        Comment comment;
        char message[MESSAGE_MAX + 1];
    };

    // Words in upper case, N words dropped. Assignments are evaluated but
    // not applied. nullptr on success, else the reason.
    const char *parseLine(const char *text, Context &ctx, Line &out);

    // The O-word of a line, nothing evaluated. False if it has none.
    bool scanOword(const char *text, OKind &kind, char (&label)[LABEL_MAX + 1]);

    // A program name as an O-word label: lower case, no spaces.
    bool routineLabel(const char *name, size_t len, char (&label)[LABEL_MAX + 1]);

    // After the line's words, as LinuxCNC does.
    const char *applyAssigns(const Line &line, Params &params, ParamSource *source);

    // (PRINT, ...) text with each #n / #<name> replaced by its value.
    const char *formatPrint(const char *message, Context &ctx, char *out, size_t outMax);
}

#endif
