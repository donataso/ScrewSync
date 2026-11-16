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
#include "GcodeMdi.hpp"
#include <string.h>

namespace
{
    using namespace Gcode;

    struct Buffer : StepSink
    {
        Step steps[Mdi::MAX_LINE_STEPS];
        size_t count = 0;
        void emit(const Step &step) override
        {
            if (count < Mdi::MAX_LINE_STEPS) {
                steps[count++] = step;
            }
        }
    };

    bool isProgramFlow(StepKind kind)
    {
        return kind == StepKind::Pause || kind == StepKind::OptionalPause || kind == StepKind::End;
    }
}

namespace Gcode
{
    bool isMdiLine(const char *text)
    {
        if (text[0] == '#' || text[0] == '(') {
            return true;
        }
        return text[0] && strchr("gmtfxzGMTFXZ", text[0]) && text[1] && strchr("-+.0123456789", text[1]);
    }

    void Mdi::begin(Dialect &dialect, int32_t zDu, int32_t xDu, Params &params, ParamSource *source, PrintSink *printer)
    {
        dialect_ = &dialect;
        params_ = &params;
        source_ = source;
        printer_ = printer;
        lineNo_ = 0;
        dialect.begin(interp_, zDu, xDu);
        interp_.setMdi(true);
    }

    void Mdi::setPosition(int32_t zDu, int32_t xDu)
    {
        interp_.setPosition(zDu, xDu);
    }

    void Mdi::setWcs(const WcsState &wcs)
    {
        interp_.setWcs(wcs);
    }

    void Mdi::probeDone(int32_t tripZDu, int32_t tripXDu, bool found, int32_t motorDu)
    {
        double scale = interp_.unitScaleDu();
        params_->setProbe(tripZDu / scale, tripXDu / scale, found, motorDu / scale);
    }

    const char *Mdi::line(const char *text, bool ui, size_t room, StepSink &sink)
    {
        Interpreter work = interp_;
        if (ui) {
            work.begin(interp_.zDu(), interp_.xDu());
        }
        static Line parsed;
        Context ctx{params_, source_, work.unitScaleDu()};
        const char *err = parseLine(text, ctx, parsed);
        if (err) {
            return err;
        }
        if (parsed.okind != OKind::None) {
            return "O-word not in MDI";
        }
        char printText[MESSAGE_MAX * 2];
        bool printLine = parsed.comment == Comment::Print && printer_;
        if (printLine) {
            err = formatPrint(parsed.message, ctx, printText, sizeof(printText));
            if (err) {
                return err;
            }
        }
        Buffer buf;
        ++lineNo_;
        if (parsed.words.count) {
            err = dialect_->translate(parsed.words);
            if (!err) {
                err = work.line(parsed.words, lineNo_, buf);
            }
            if (err) {
                return err;
            }
        }
        size_t stepCount = 0;
        for (size_t i = 0; i < buf.count; i++) {
            stepCount += !isProgramFlow(buf.steps[i].kind);
        }
        stepCount += printLine;
        if (stepCount > room) {
            return "queue full";
        }
        err = applyAssigns(parsed, *params_, source_);
        if (err) {
            return err;
        }
        if (printLine) {
            printer_->print(printText);
            Step printStep = {};
            printStep.kind = StepKind::Print;
            printStep.line = lineNo_;
            sink.emit(printStep);
        }
        if (ui) {
            interp_.setPosition(work.zDu(), work.xDu());
            interp_.setWcs(work.wcs());
        } else {
            interp_ = work;
        }
        for (size_t i = 0; i < buf.count; i++) {
            if (!isProgramFlow(buf.steps[i].kind)) {
                sink.emit(buf.steps[i]);
            }
        }
        return nullptr;
    }
}
