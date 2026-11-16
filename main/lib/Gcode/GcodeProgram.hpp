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
#ifndef GcodeProgram_h
#define GcodeProgram_h

#include "GcodeCore.hpp"
#include "GcodeInterpreter.hpp"
#include "GcodeNgc.hpp"

// Runs a program line by line at execution time, with O-word flow and
// routine calls. A line is read only after the steps before it are done.
namespace Gcode
{
    struct RoutineSource
    {
        // Text that defines o<label> sub; nullptr if none. Valid while the
        // program runs.
        virtual const char *find(const char *label, size_t &len) = 0;
        virtual ~RoutineSource()
        {}
    };

    class Program
    {
    public:
        enum class Status : uint8_t
        {
            Steps, // the line's steps went to the sink
            Busy,  // line budget used, no step yet
            End,
            Error,
        };

        struct Setup
        {
            Dialect *dialect;
            Params *params;
            ParamSource *source;
            RoutineSource *routines;
            bool dryRun; // ends where values are not known: see unknown()
            PrintSink *printer = nullptr; // (PRINT, ...) lines
        };

        void begin(const Setup &setup, const char *text, size_t len, int32_t startZDu, int32_t startXDu, const WcsState &wcs = {});
        Status next(StepSink &sink, uint32_t maxLines);

        // Error: the reason, the routine ("" = the program) and its line.
        // An ABORT has line 0.
        const char *error() const
        {
            return error_;
        }
        const char *errorRoutine() const
        {
            return errorRoutine_;
        }
        uint32_t errorLine() const
        {
            return errorLine_;
        }
        // The last (MSG, ...) text.
        const char *message() const
        {
            return message_;
        }
        bool unknown() const
        {
            return unknown_;
        }

        // After a G38 move: where the axes are, the trip point, and the
        // probed axis's motor position there.
        void probeDone(int32_t zDu, int32_t xDu, int32_t tripZDu, int32_t tripXDu, bool found, int32_t motorDu);

    private:
        struct Frame
        {
            const char *text;
            size_t len;
            size_t pos;
            uint32_t lineNo;
            char label[LABEL_MAX + 1]; // the sub
            char file[LABEL_MAX + 1];  // the routine, "" = the program
            size_t blockBase;
        };

        struct Block
        {
            OKind kind;
            char label[LABEL_MAX + 1];
            size_t pos;
            uint32_t lineNo;
            int32_t remaining;
        };

        static constexpr size_t BLOCK_MAX = 16;

        Status fail(const char *reason);
        Status oword();
        bool skipTo(OKind a, OKind b, OKind &found);
        bool call();
        bool push(OKind kind, size_t pos, uint32_t lineNo, int32_t remaining);
        Block *top(OKind kind);

        Setup setup_;
        Interpreter interp_;
        Frame frames_[CALL_DEPTH + 1];
        size_t depth_;
        Block blocks_[BLOCK_MAX];
        size_t blockCount_;
        Line line_;
        size_t lineStart_;
        const char *error_;
        char errorText_[MESSAGE_MAX + 1];
        char errorRoutine_[LABEL_MAX + 1];
        uint32_t errorLine_;
        char message_[MESSAGE_MAX + 1];
        bool unknown_;
    };

    // Reads the line at pos into buf and moves pos past it. False if too long.
    bool readLine(const char *text, size_t len, size_t &pos, char (&buf)[MAX_LINE_LEN]);

    // Where o<label> sub starts in text: the line after it, and its line number.
    bool findSub(const char *text, size_t len, const char *label, size_t &pos, uint32_t &lineNo);

    // Every line parses (nothing evaluated), O-words match up and subs are
    // not nested. nullptr on success, else the reason and line.
    const char *checkProgram(const char *text, size_t len, uint32_t &line);
}

#endif
