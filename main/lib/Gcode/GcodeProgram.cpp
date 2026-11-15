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
#include "GcodeProgram.hpp"
#include <math.h>
#include <string.h>

namespace
{
    using namespace Gcode;

    struct Counter : StepSink
    {
        StepSink &sink;
        size_t count = 0;
        explicit Counter(StepSink &s) : sink(s)
        {}
        void emit(const Step &step) override
        {
            count++;
            sink.emit(step);
        }
    };

    template <size_t N>
    void copyStr(char (&out)[N], const char *s)
    {
        strncpy(out, s, N - 1);
        out[N - 1] = '\0';
    }

    OKind opener(OKind end)
    {
        switch (end) {
        case OKind::Endsub:
            return OKind::Sub;
        case OKind::Endif:
        case OKind::Else:
            return OKind::If;
        case OKind::Endwhile:
            return OKind::While;
        case OKind::Endrepeat:
            return OKind::Repeat;
        default:
            return OKind::None;
        }
    }
}

namespace Gcode
{
    bool readLine(const char *text, size_t len, size_t &pos, char (&buf)[MAX_LINE_LEN])
    {
        size_t start = pos;
        size_t end = start;
        while (end < len && text[end] != '\n') {
            end++;
        }
        pos = end < len ? end + 1 : end;
        size_t n = end - start;
        if (n && text[start + n - 1] == '\r') {
            n--;
        }
        if (n >= MAX_LINE_LEN) {
            buf[0] = '\0';
            return false;
        }
        memcpy(buf, text + start, n);
        buf[n] = '\0';
        return true;
    }

    bool findSub(const char *text, size_t len, const char *label, size_t &pos, uint32_t &lineNo)
    {
        size_t p = 0;
        uint32_t n = 0;
        char buf[MAX_LINE_LEN];
        while (p < len) {
            bool ok = readLine(text, len, p, buf);
            n++;
            OKind kind;
            char found[LABEL_MAX + 1];
            if (ok && scanOword(buf, kind, found) && kind == OKind::Sub && !strcmp(found, label)) {
                pos = p;
                lineNo = n;
                return true;
            }
        }
        return false;
    }

    const char *checkProgram(const char *text, size_t len, uint32_t &line)
    {
        struct Open
        {
            OKind kind;
            char label[LABEL_MAX + 1];
            uint32_t line;
            bool hasElse;
        };
        static Open stack[32];
        static Line parsed;
        size_t depth = 0;
        size_t pos = 0;
        line = 0;
        char buf[MAX_LINE_LEN];
        while (pos < len) {
            line++;
            if (!readLine(text, len, pos, buf)) {
                return "line too long";
            }
            Context ctx{nullptr, nullptr, 1};
            ctx.check = true;
            const char *err = parseLine(buf, ctx, parsed);
            if (err) {
                return err;
            }
            OKind kind = parsed.okind;
            Open *top = depth ? &stack[depth - 1] : nullptr;
            switch (kind) {
            case OKind::None:
            case OKind::Call:
                break;
            case OKind::Sub:
            case OKind::If:
            case OKind::While:
            case OKind::Repeat:
                if (kind == OKind::Sub && depth) {
                    return "sub inside a block";
                }
                if (depth >= sizeof(stack) / sizeof(stack[0])) {
                    return "blocks too deep";
                }
                stack[depth].kind = kind;
                copyStr(stack[depth].label, parsed.label);
                stack[depth].line = line;
                stack[depth].hasElse = false;
                depth++;
                break;
            case OKind::Else:
                if (!top || top->kind != OKind::If || strcmp(top->label, parsed.label) != 0 || top->hasElse) {
                    return "unmatched else";
                }
                top->hasElse = true;
                break;
            default:
                if (!top || top->kind != opener(kind) || strcmp(top->label, parsed.label) != 0) {
                    return "unmatched O-word";
                }
                depth--;
                break;
            }
        }
        if (depth) {
            line = stack[depth - 1].line;
            return "O-word not closed";
        }
        return nullptr;
    }

    void Program::begin(const Setup &setup, const char *text, size_t len, int32_t startZDu, int32_t startXDu)
    {
        setup_ = setup;
        setup_.dialect->begin(interp_, startZDu, startXDu);
        setup_.params->resetCalls();
        depth_ = 0;
        blockCount_ = 0;
        frames_[0] = {text, len, 0, 0, "", "", 0};
        error_ = nullptr;
        errorRoutine_[0] = '\0';
        errorLine_ = 0;
        message_[0] = '\0';
        unknown_ = false;
    }

    Program::Status Program::fail(const char *reason)
    {
        error_ = reason;
        errorLine_ = frames_[depth_].lineNo;
        copyStr(errorRoutine_, frames_[depth_].file);
        return Status::Error;
    }

    Program::Block *Program::top(OKind kind)
    {
        if (blockCount_ <= frames_[depth_].blockBase) {
            return nullptr;
        }
        Block &b = blocks_[blockCount_ - 1];
        return b.kind == kind && !strcmp(b.label, line_.label) ? &b : nullptr;
    }

    bool Program::push(OKind kind, size_t pos, uint32_t lineNo, int32_t remaining)
    {
        if (blockCount_ >= BLOCK_MAX) {
            fail("blocks too deep");
            return false;
        }
        Block &b = blocks_[blockCount_++];
        b.kind = kind;
        copyStr(b.label, line_.label);
        b.pos = pos;
        b.lineNo = lineNo;
        b.remaining = remaining;
        return true;
    }

    bool Program::skipTo(OKind a, OKind b, OKind &found)
    {
        Frame &f = frames_[depth_];
        char buf[MAX_LINE_LEN];
        while (f.pos < f.len) {
            bool ok = readLine(f.text, f.len, f.pos, buf);
            f.lineNo++;
            OKind kind;
            char label[LABEL_MAX + 1];
            if (ok && scanOword(buf, kind, label) && (kind == a || kind == b) && !strcmp(label, line_.label)) {
                found = kind;
                return true;
            }
        }
        return false;
    }

    bool Program::call()
    {
        if (depth_ >= CALL_DEPTH) {
            fail("calls too deep");
            return false;
        }
        const Frame &f = frames_[depth_];
        const char *text = f.text;
        size_t len = f.len;
        const char *file = f.file;
        size_t pos;
        uint32_t lineNo;
        bool found = findSub(text, len, line_.label, pos, lineNo);
        if (!found && depth_) {
            text = frames_[0].text;
            len = frames_[0].len;
            file = frames_[0].file;
            found = findSub(text, len, line_.label, pos, lineNo);
        }
        if (!found) {
            if (setup_.dryRun) {
                unknown_ = true;
                return false;
            }
            text = setup_.routines ? setup_.routines->find(line_.label, len) : nullptr;
            if (!text) {
                fail("unknown routine");
                return false;
            }
            if (!findSub(text, len, line_.label, pos, lineNo)) {
                fail("routine has no sub");
                return false;
            }
            file = line_.label;
        }
        if (!setup_.params->enter(line_.args, line_.argCount)) {
            fail("calls too deep");
            return false;
        }
        Frame &next = frames_[++depth_];
        next.text = text;
        next.len = len;
        next.pos = pos;
        next.lineNo = lineNo;
        copyStr(next.label, line_.label);
        copyStr(next.file, file);
        next.blockBase = blockCount_;
        return true;
    }

    Program::Status Program::oword()
    {
        Frame &f = frames_[depth_];
        OKind found;
        switch (line_.okind) {
        case OKind::Sub:
            return skipTo(OKind::Endsub, OKind::Endsub, found) ? Status::Busy : fail("missing endsub");
        case OKind::Endsub:
            if (!depth_ || strcmp(f.label, line_.label) != 0) {
                return fail("endsub without call");
            }
            blockCount_ = f.blockBase;
            depth_--;
            setup_.params->leave();
            return Status::Busy;
        case OKind::Call:
            if (call()) {
                return Status::Busy;
            }
            return unknown_ ? Status::End : Status::Error;
        case OKind::If:
            if (line_.args[0] != 0) {
                return push(OKind::If, 0, 0, 0) ? Status::Busy : Status::Error;
            }
            if (!skipTo(OKind::Else, OKind::Endif, found)) {
                return fail("missing endif");
            }
            if (found == OKind::Else && !push(OKind::If, 0, 0, 0)) {
                return Status::Error;
            }
            return Status::Busy;
        case OKind::Else:
            if (!top(OKind::If)) {
                return fail("unexpected else");
            }
            blockCount_--;
            return skipTo(OKind::Endif, OKind::Endif, found) ? Status::Busy : fail("missing endif");
        case OKind::Endif:
            if (!top(OKind::If)) {
                return fail("unexpected endif");
            }
            blockCount_--;
            return Status::Busy;
        case OKind::While:
            if (line_.args[0] != 0) {
                return push(OKind::While, lineStart_, f.lineNo - 1, 0) ? Status::Busy : Status::Error;
            }
            return skipTo(OKind::Endwhile, OKind::Endwhile, found) ? Status::Busy : fail("missing endwhile");
        case OKind::Endwhile:
        {
            Block *b = top(OKind::While);
            if (!b) {
                return fail("unexpected endwhile");
            }
            f.pos = b->pos;
            f.lineNo = b->lineNo;
            blockCount_--;
            return Status::Busy;
        }
        case OKind::Repeat:
        {
            double count = round(line_.args[0]);
            if (count > 0) {
                int32_t n = count > 1e9 ? 1000000000 : (int32_t)count;
                return push(OKind::Repeat, f.pos, f.lineNo, n) ? Status::Busy : Status::Error;
            }
            return skipTo(OKind::Endrepeat, OKind::Endrepeat, found) ? Status::Busy : fail("missing endrepeat");
        }
        case OKind::Endrepeat:
        {
            Block *b = top(OKind::Repeat);
            if (!b) {
                return fail("unexpected endrepeat");
            }
            if (--b->remaining > 0) {
                f.pos = b->pos;
                f.lineNo = b->lineNo;
            } else {
                blockCount_--;
            }
            return Status::Busy;
        }
        case OKind::None:
            break;
        }
        return Status::Busy;
    }

    Program::Status Program::next(StepSink &sink, uint32_t maxLines)
    {
        for (uint32_t n = 0; n < maxLines; n++) {
            Frame &f = frames_[depth_];
            if (f.pos >= f.len) {
                return depth_ ? fail("missing endsub") : Status::End;
            }
            char buf[MAX_LINE_LEN];
            lineStart_ = f.pos;
            f.lineNo++;
            if (!readLine(f.text, f.len, f.pos, buf)) {
                return fail("line too long");
            }
            Context ctx{setup_.params, setup_.source, interp_.unitScaleDu()};
            ctx.dryRun = setup_.dryRun;
            const char *err = parseLine(buf, ctx, line_);
            if (ctx.unknown) {
                unknown_ = true;
                return Status::End;
            }
            if (err) {
                return fail(err);
            }
            if (line_.okind != OKind::None) {
                Status s = oword();
                if (s != Status::Busy) {
                    return s;
                }
                continue;
            }
            if (line_.comment == Comment::Abort) {
                if (setup_.dryRun) {
                    unknown_ = true;
                    return Status::End;
                }
                copyStr(errorText_, line_.message);
                error_ = errorText_;
                errorLine_ = 0;
                errorRoutine_[0] = '\0';
                return Status::Error;
            }
            if (line_.comment == Comment::Msg) {
                copyStr(message_, line_.message);
            }
            if (line_.comment == Comment::Print && setup_.printer && !setup_.dryRun) {
                char text[MESSAGE_MAX * 2];
                formatPrint(line_.message, ctx, text, sizeof(text));
                setup_.printer->print(text);
            }
            Counter counter(sink);
            if (line_.words.count) {
                err = setup_.dialect->translate(line_.words);
                if (!err) {
                    err = interp_.line(line_.words, f.lineNo, counter);
                }
                if (err) {
                    return fail(err);
                }
            }
            err = applyAssigns(line_, *setup_.params, setup_.source);
            if (err) {
                return fail(err);
            }
            if (counter.count) {
                return Status::Steps;
            }
        }
        return Status::Busy;
    }

    void Program::probeDone(int32_t zDu, int32_t xDu, int32_t tripZDu, int32_t tripXDu, bool found, int32_t motorDu)
    {
        interp_.setPosition(zDu, xDu);
        double scale = interp_.unitScaleDu();
        setup_.params->setProbe(tripZDu / scale, tripXDu / scale, found, motorDu / scale);
    }
}
