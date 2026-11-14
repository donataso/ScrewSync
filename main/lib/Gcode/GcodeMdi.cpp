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

    bool isMotion(StepKind kind)
    {
        return kind == StepKind::Rapid || kind == StepKind::Feed || kind == StepKind::Thread;
    }
}

namespace Gcode
{
    bool isMdiLine(const char *text)
    {
        return text[0] && strchr("gmtfxzGMTFXZ", text[0]) && text[1] && strchr("-+.0123456789", text[1]);
    }

    void Mdi::begin(Dialect &dialect, int32_t zDu, int32_t xDu)
    {
        dialect_ = &dialect;
        lineNo_ = 0;
        dialect.begin(interp_, zDu, xDu);
    }

    void Mdi::setPosition(int32_t zDu, int32_t xDu)
    {
        interp_.setPosition(zDu, xDu);
    }

    const char *Mdi::line(const char *text, bool ui, size_t room, StepSink &sink)
    {
        Interpreter work = interp_;
        if (ui) {
            work.begin(interp_.zDu(), interp_.xDu());
        }
        Buffer buf;
        const char *err = interpretLine(*dialect_, work, text, ++lineNo_, buf);
        if (err) {
            return err;
        }
        size_t motionCount = 0;
        for (size_t i = 0; i < buf.count; i++) {
            motionCount += isMotion(buf.steps[i].kind);
        }
        if (motionCount > room) {
            return "queue full";
        }
        if (ui) {
            interp_.setPosition(work.zDu(), work.xDu());
        } else {
            interp_ = work;
        }
        for (size_t i = 0; i < buf.count; i++) {
            if (isMotion(buf.steps[i].kind)) {
                sink.emit(buf.steps[i]);
            }
        }
        return nullptr;
    }
}
