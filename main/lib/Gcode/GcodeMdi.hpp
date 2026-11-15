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
#ifndef GcodeMdi_h
#define GcodeMdi_h

#include "GcodeCore.hpp"
#include "GcodeInterpreter.hpp"
#include "GcodeNgc.hpp"

namespace Gcode
{
    constexpr char MDI_UI_MARKER = '@';

    bool isMdiLine(const char *text);

    // One MDI session: modal state kept between lines.
    class Mdi
    {
    public:
        static constexpr size_t MAX_LINE_STEPS = 4;

        void begin(Dialect &dialect, int32_t zDu, int32_t xDu, Params &params, ParamSource *source);
        void setPosition(int32_t zDu, int32_t xDu);
        // After a G38 move: the trip point, work du, and the probed axis's
        // motor position, du.
        void probeDone(int32_t tripZDu, int32_t tripXDu, bool found, int32_t motorDu);
        int32_t zDu() const
        {
            return interp_.zDu();
        }
        int32_t xDu() const
        {
            return interp_.xDu();
        }

        // Steps go to sink, all or none, at most room of them; M0/M1/M2/M30 do
        // nothing. ui: a fresh interpreter at the session position runs the
        // line, so only the position carries back. Parameters and
        // expressions are evaluated now; O-words are refused.
        const char *line(const char *text, bool ui, size_t room, StepSink &sink);

    private:
        Dialect *dialect_;
        Interpreter interp_;
        uint32_t lineNo_;
        Params *params_;
        ParamSource *source_;
    };
}

#endif
