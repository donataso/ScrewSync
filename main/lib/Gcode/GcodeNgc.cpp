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
#include "GcodeNgc.hpp"
#include <math.h>
#include <stdio.h>
#include <string.h>

namespace
{
    using namespace Gcode;

    constexpr int DEPTH_MAX = 16;
    constexpr double TOLERANCE_EQUAL = 0.0001;
    constexpr double DEG = 3.14159265358979323846 / 180.0;
    constexpr uint8_t GLOBAL = 0xFF;
    constexpr const char *PROBE_MOTOR = "_probe_motor";

    bool isSpace(char c)
    {
        return c == ' ' || c == '\t' || c == '\r' || c == '\n';
    }

    bool isDigit(char c)
    {
        return c >= '0' && c <= '9';
    }

    bool isAlpha(char c)
    {
        return c >= 'a' && c <= 'z';
    }

    char lower(char c)
    {
        return c >= 'A' && c <= 'Z' ? (char)(c - 'A' + 'a') : c;
    }

    bool startsWithCI(const char *s, const char *prefix)
    {
        for (; *prefix; s++, prefix++) {
            if (lower(*s) != *prefix) {
                return false;
            }
        }
        return true;
    }

    // Comment text "MSG, x" / "ABORT, x" into out.
    void takeComment(const char *c, size_t len, Line &out)
    {
        while (len && isSpace(*c)) {
            c++;
            len--;
        }
        Comment kind = Comment::None;
        size_t skip = 0;
        if (len >= 4 && startsWithCI(c, "msg,")) {
            kind = Comment::Msg;
            skip = 4;
        } else if (len >= 6 && startsWithCI(c, "abort,")) {
            kind = Comment::Abort;
            skip = 6;
        } else if (len >= 6 && startsWithCI(c, "print,")) {
            kind = Comment::Print;
            skip = 6;
        }
        if (kind == Comment::None || out.comment != Comment::None) {
            return;
        }
        c += skip;
        len -= skip;
        while (len && isSpace(*c)) {
            c++;
            len--;
        }
        while (len && isSpace(c[len - 1])) {
            len--;
        }
        if (len > MESSAGE_MAX) {
            len = MESSAGE_MAX;
        }
        memcpy(out.message, c, len);
        out.message[len] = '\0';
        out.comment = kind;
    }

    // Comments out, spaces out, lower case. False if too long.
    bool prepare(const char *text, char (&buf)[MAX_LINE_LEN], Line *comments)
    {
        size_t n = 0;
        const char *commentStart = nullptr;
        for (const char *p = text;; p++) {
            char c = *p;
            if (commentStart) {
                if (!c || c == ')') {
                    if (comments) {
                        takeComment(commentStart, (size_t)(p - commentStart), *comments);
                    }
                    commentStart = nullptr;
                }
                if (!c) {
                    break;
                }
                continue;
            }
            if (!c || c == ';') {
                break;
            }
            if (c == '(') {
                commentStart = p + 1;
                continue;
            }
            if (isSpace(c)) {
                continue;
            }
            if (n + 1 >= sizeof(buf)) {
                return false;
            }
            buf[n++] = lower(c);
        }
        buf[n] = '\0';
        if (buf[0] == '%' && buf[1] == '\0') {
            buf[0] = '\0';
        }
        return true;
    }

    bool readNumber(const char *&p, double &out)
    {
        const char *s = p;
        double intPart = 0;
        bool any = false;
        while (isDigit(*s)) {
            intPart = intPart * 10 + (*s - '0');
            any = true;
            s++;
        }
        double frac = 0;
        double scale = 1;
        if (*s == '.') {
            s++;
            while (isDigit(*s)) {
                scale *= 10;
                frac = frac * 10 + (*s - '0');
                any = true;
                s++;
            }
        }
        if (!any) {
            return false;
        }
        out = intPart + frac / scale;
        p = s;
        return true;
    }

    bool readName(const char *&p, char (&out)[LABEL_MAX + 1])
    {
        if (*p != '<') {
            return false;
        }
        const char *s = p + 1;
        size_t n = 0;
        while (*s && *s != '>') {
            if (n >= LABEL_MAX) {
                return false;
            }
            out[n++] = *s++;
        }
        if (*s != '>' || n == 0) {
            return false;
        }
        out[n] = '\0';
        p = s + 1;
        return true;
    }

    bool integral(double v, int32_t &out)
    {
        double r = round(v);
        if (fabs(v - r) > 1e-6 || r > 1e9 || r < -1e9) {
            return false;
        }
        out = (int32_t)r;
        return true;
    }

    enum class Op : uint8_t
    {
        None,
        Pow,
        Mul,
        Div,
        Mod,
        Add,
        Sub,
        Eq,
        Ne,
        Gt,
        Ge,
        Lt,
        Le,
        And,
        Or,
        Xor,
    };

    int precedence(Op op)
    {
        switch (op) {
        case Op::Pow:
            return 4;
        case Op::Mul:
        case Op::Div:
        case Op::Mod:
            return 3;
        case Op::Add:
        case Op::Sub:
            return 2;
        case Op::And:
        case Op::Or:
        case Op::Xor:
            return 0;
        case Op::None:
            return -1;
        default:
            return 1;
        }
    }

    struct Keyword
    {
        const char *text;
        Op op;
    };

    const Keyword OPS[] = {
        {"**", Op::Pow},
        {"*", Op::Mul},
        {"/", Op::Div},
        {"mod", Op::Mod},
        {"+", Op::Add},
        {"-", Op::Sub},
        {"eq", Op::Eq},
        {"ne", Op::Ne},
        {"gt", Op::Gt},
        {"ge", Op::Ge},
        {"lt", Op::Lt},
        {"le", Op::Le},
        {"and", Op::And},
        {"or", Op::Or},
        {"xor", Op::Xor},
    };

    Op peekOp(const char *p, size_t &len)
    {
        for (const Keyword &k : OPS) {
            size_t n = strlen(k.text);
            if (!strncmp(p, k.text, n)) {
                len = n;
                return k.op;
            }
        }
        len = 0;
        return Op::None;
    }

    struct Parser
    {
        const char *p;
        Context &ctx;
        int depth;

        const char *fail(const char *reason, double &out)
        {
            if (ctx.check) {
                out = 0;
                return nullptr;
            }
            return reason;
        }

        const char *apply(Op op, double a, double b, double &out)
        {
            switch (op) {
            case Op::Pow:
                out = pow(a, b);
                break;
            case Op::Mul:
                out = a * b;
                break;
            case Op::Div:
                if (b == 0) {
                    return fail("division by zero", out);
                }
                out = a / b;
                break;
            case Op::Mod:
                if (b == 0) {
                    return fail("division by zero", out);
                }
                out = fmod(a, b);
                if (out < 0) {
                    out += fabs(b);
                }
                break;
            case Op::Add:
                out = a + b;
                break;
            case Op::Sub:
                out = a - b;
                break;
            case Op::Eq:
                out = fabs(a - b) < TOLERANCE_EQUAL;
                break;
            case Op::Ne:
                out = fabs(a - b) >= TOLERANCE_EQUAL;
                break;
            case Op::Gt:
                out = a > b;
                break;
            case Op::Ge:
                out = a >= b;
                break;
            case Op::Lt:
                out = a < b;
                break;
            case Op::Le:
                out = a <= b;
                break;
            case Op::And:
                out = a != 0 && b != 0;
                break;
            case Op::Or:
                out = a != 0 || b != 0;
                break;
            case Op::Xor:
                out = (a != 0) != (b != 0);
                break;
            case Op::None:
                break;
            }
            if (!isfinite(out)) {
                return fail("bad math", out);
            }
            return nullptr;
        }

        const char *binary(int minPrec, double &lhs)
        {
            while (true) {
                size_t len;
                Op op = peekOp(p, len);
                if (op == Op::None || precedence(op) < minPrec) {
                    return nullptr;
                }
                p += len;
                double rhs;
                const char *err = value(rhs);
                if (err) {
                    return err;
                }
                size_t nextLen;
                Op next = peekOp(p, nextLen);
                if (next != Op::None && precedence(next) > precedence(op)) {
                    err = binary(precedence(op) + 1, rhs);
                    if (err) {
                        return err;
                    }
                }
                err = apply(op, lhs, rhs, lhs);
                if (err) {
                    return err;
                }
            }
        }

        const char *expression(double &out)
        {
            if (*p != '[') {
                return "bad expression";
            }
            if (++depth > DEPTH_MAX) {
                return "expression too deep";
            }
            p++;
            const char *err = value(out);
            if (!err) {
                err = binary(0, out);
            }
            if (!err && *p != ']') {
                err = "bad expression";
            }
            p++;
            depth--;
            return err;
        }

        const char *function(double &out)
        {
            const char *start = p;
            while (isAlpha(*p)) {
                p++;
            }
            size_t len = (size_t)(p - start);
            double a;
            const char *err = expression(a);
            if (err) {
                return err;
            }
            auto is = [&](const char *name) {
                return strlen(name) == len && !strncmp(start, name, len);
            };
            if (is("atan")) {
                if (*p != '/') {
                    return "bad expression";
                }
                p++;
                double b;
                err = expression(b);
                if (err) {
                    return err;
                }
                out = atan2(a, b) / DEG;
                return nullptr;
            }
            if (is("abs")) {
                out = fabs(a);
            } else if (is("acos") || is("asin")) {
                if (a < -1 || a > 1) {
                    return fail("bad math", out);
                }
                out = (is("acos") ? acos(a) : asin(a)) / DEG;
            } else if (is("cos")) {
                out = cos(a * DEG);
            } else if (is("sin")) {
                out = sin(a * DEG);
            } else if (is("tan")) {
                out = tan(a * DEG);
            } else if (is("exp")) {
                out = exp(a);
            } else if (is("fix")) {
                out = floor(a);
            } else if (is("fup")) {
                out = ceil(a);
            } else if (is("round")) {
                out = round(a);
            } else if (is("ln")) {
                if (a <= 0) {
                    return fail("bad math", out);
                }
                out = log(a);
            } else if (is("sqrt")) {
                if (a < 0) {
                    return fail("bad math", out);
                }
                out = sqrt(a);
            } else {
                return "unknown function";
            }
            if (!isfinite(out)) {
                return fail("bad math", out);
            }
            return nullptr;
        }

        // After '#': a name or a number.
        const char *reference(int32_t &number, char (&name)[LABEL_MAX + 1])
        {
            if (*p == '<') {
                number = 0;
                return readName(p, name) ? nullptr : "bad parameter name";
            }
            double v;
            const char *err = value(v);
            if (err) {
                return err;
            }
            if (!integral(v, number) || number <= 0) {
                if (ctx.check) {
                    number = 1;
                    return nullptr;
                }
                return "bad parameter";
            }
            name[0] = '\0';
            return nullptr;
        }

        const char *read(int32_t number, const char *name, double &out)
        {
            out = 0;
            if (ctx.check) {
                return nullptr;
            }
            if (number) {
                if (number >= PROBE_FIRST && number <= PROBE_LAST && ctx.dryRun) {
                    ctx.unknown = true;
                    return nullptr;
                }
                return ctx.params->numbered(number, out);
            }
            if (name[0] == '_' && ctx.source) {
                bool length = false;
                bool live = false;
                if (ctx.source->get(name, out, length, live)) {
                    if (length) {
                        out /= ctx.unitScaleDu;
                    }
                    if (live && ctx.dryRun) {
                        ctx.unknown = true;
                    }
                    return nullptr;
                }
            }
            if (!strcmp(name, PROBE_MOTOR) && ctx.dryRun) {
                ctx.unknown = true;
                return nullptr;
            }
            if (ctx.params->named(name, out)) {
                return nullptr;
            }
            if (ctx.dryRun) {
                ctx.unknown = true;
                return nullptr;
            }
            return "undefined parameter";
        }

        const char *value(double &out)
        {
            int sign = 1;
            if (*p == '-' || *p == '+') {
                sign = *p == '-' ? -1 : 1;
                p++;
            }
            const char *err = nullptr;
            if (*p == '[') {
                err = expression(out);
            } else if (*p == '#') {
                p++;
                if (++depth > DEPTH_MAX) {
                    return "expression too deep";
                }
                int32_t number;
                char name[LABEL_MAX + 1];
                err = reference(number, name);
                if (!err) {
                    err = read(number, name, out);
                }
                depth--;
            } else if (isAlpha(*p)) {
                err = function(out);
            } else if (!readNumber(p, out)) {
                err = "malformed word";
            }
            out *= sign;
            return err;
        }
    };

    struct OwordName
    {
        const char *text;
        OKind kind;
    };

    const OwordName OWORDS[] = {
        {"sub", OKind::Sub},
        {"endsub", OKind::Endsub},
        {"call", OKind::Call},
        {"if", OKind::If},
        {"else", OKind::Else},
        {"endif", OKind::Endif},
        {"while", OKind::While},
        {"endwhile", OKind::Endwhile},
        {"repeat", OKind::Repeat},
        {"endrepeat", OKind::Endrepeat},
    };

    // At 'o': label and keyword.
    const char *owordHead(const char *&p, OKind &kind, char (&label)[LABEL_MAX + 1])
    {
        p++;
        if (*p == '<') {
            if (!readName(p, label)) {
                return "bad O-word";
            }
        } else {
            size_t n = 0;
            while (isDigit(*p)) {
                if (n >= LABEL_MAX) {
                    return "bad O-word";
                }
                label[n++] = *p++;
            }
            if (!n) {
                return "bad O-word";
            }
            label[n] = '\0';
        }
        const char *start = p;
        while (isAlpha(*p)) {
            p++;
        }
        size_t len = (size_t)(p - start);
        for (const OwordName &o : OWORDS) {
            if (strlen(o.text) == len && !strncmp(start, o.text, len)) {
                kind = o.kind;
                return nullptr;
            }
        }
        return "unsupported O-word";
    }

    // Past a leading N word.
    const char *skipLineNumber(const char *p)
    {
        if (*p == 'n') {
            const char *s = p + 1;
            double v;
            if (readNumber(s, v)) {
                return s;
            }
        }
        return p;
    }

    const char *oword(Parser &ps, Line &out)
    {
        const char *err = owordHead(ps.p, out.okind, out.label);
        if (err) {
            return err;
        }
        bool oneArg = out.okind == OKind::If || out.okind == OKind::While || out.okind == OKind::Repeat;
        while (*ps.p == '[') {
            if (out.okind != OKind::Call && !oneArg) {
                return "unexpected O-word value";
            }
            if (out.argCount >= (oneArg ? 1 : LOCAL_COUNT)) {
                return "too many arguments";
            }
            err = ps.expression(out.args[out.argCount++]);
            if (err) {
                return err;
            }
        }
        if (*ps.p) {
            return "malformed O-word";
        }
        if (oneArg && out.argCount != 1) {
            return "O-word needs a value";
        }
        return nullptr;
    }
}

namespace Gcode
{
    void Params::clear()
    {
        memset(locals_, 0, sizeof(locals_));
        memset(probe_, 0, sizeof(probe_));
        probeMotor_ = 0;
        namedCount_ = 0;
        depth_ = 0;
    }

    void Params::resetCalls()
    {
        while (depth_) {
            leave();
        }
    }

    bool Params::enter(const double *args, size_t count)
    {
        if (depth_ >= CALL_DEPTH) {
            return false;
        }
        depth_++;
        for (size_t i = 0; i < LOCAL_COUNT; i++) {
            locals_[depth_][i] = i < count ? args[i] : 0;
        }
        return true;
    }

    void Params::leave()
    {
        if (!depth_) {
            return;
        }
        size_t kept = 0;
        for (size_t i = 0; i < namedCount_; i++) {
            if (named_[i].level != depth_) {
                named_[kept++] = named_[i];
            }
        }
        namedCount_ = kept;
        depth_--;
    }

    uint8_t Params::levelOf(const char *name) const
    {
        return name[0] == '_' ? GLOBAL : depth_;
    }

    Params::Named *Params::find(const char *name, uint8_t level)
    {
        for (size_t i = 0; i < namedCount_; i++) {
            if (named_[i].level == level && !strcmp(named_[i].name, name)) {
                return &named_[i];
            }
        }
        return nullptr;
    }

    const Params::Named *Params::find(const char *name, uint8_t level) const
    {
        return const_cast<Params *>(this)->find(name, level);
    }

    const char *Params::numbered(int32_t n, double &value) const
    {
        if (n >= 1 && n <= (int32_t)LOCAL_COUNT) {
            value = locals_[depth_][n - 1];
            return nullptr;
        }
        if (n >= PROBE_FIRST && n <= PROBE_LAST) {
            value = probe_[n - PROBE_FIRST];
            return nullptr;
        }
        return "unsupported parameter";
    }

    const char *Params::setNumbered(int32_t n, double value)
    {
        if (n >= 1 && n <= (int32_t)LOCAL_COUNT) {
            locals_[depth_][n - 1] = value;
            return nullptr;
        }
        if (n >= PROBE_FIRST && n <= PROBE_LAST) {
            return "read-only parameter";
        }
        return "unsupported parameter";
    }

    bool Params::named(const char *name, double &value) const
    {
        if (!strcmp(name, PROBE_MOTOR)) {
            value = probeMotor_;
            return true;
        }
        const Named *e = find(name, levelOf(name));
        if (e) {
            value = e->value;
        }
        return e != nullptr;
    }

    const char *Params::setNamed(const char *name, double value)
    {
        if (!strcmp(name, PROBE_MOTOR)) {
            return "read-only parameter";
        }
        uint8_t level = levelOf(name);
        Named *e = find(name, level);
        if (!e) {
            if (namedCount_ >= NAMED_MAX) {
                return "too many parameters";
            }
            e = &named_[namedCount_++];
            strncpy(e->name, name, LABEL_MAX);
            e->name[LABEL_MAX] = '\0';
            e->level = level;
        }
        e->value = value;
        return nullptr;
    }

    void Params::setProbe(double zValue, double xValue, bool found, double motor)
    {
        probeMotor_ = motor;
        memset(probe_, 0, sizeof(probe_));
        probe_[5061 - PROBE_FIRST] = xValue;
        probe_[5063 - PROBE_FIRST] = zValue;
        probe_[5070 - PROBE_FIRST] = found ? 1 : 0;
    }

    const char *parseLine(const char *text, Context &ctx, Line &out)
    {
        out.words.count = 0;
        out.assignCount = 0;
        out.okind = OKind::None;
        out.label[0] = '\0';
        out.argCount = 0;
        out.comment = Comment::None;
        out.message[0] = '\0';

        char buf[MAX_LINE_LEN];
        if (!prepare(text, buf, &out)) {
            return "line too long";
        }
        Parser ps{skipLineNumber(buf), ctx, 0};
        if (*ps.p == 'o') {
            return oword(ps, out);
        }
        while (*ps.p) {
            char c = *ps.p;
            if (c == '#') {
                if (out.assignCount >= ASSIGN_MAX) {
                    return "too many parameters";
                }
                Assign &a = out.assigns[out.assignCount++];
                ps.p++;
                const char *err = ps.reference(a.number, a.name);
                if (err) {
                    return err;
                }
                if (*ps.p != '=') {
                    return "malformed parameter";
                }
                ps.p++;
                err = ps.value(a.value);
                if (err) {
                    return err;
                }
                continue;
            }
            if (!isAlpha(c)) {
                return "malformed word";
            }
            if (c == 'o') {
                return "O-word not first";
            }
            ps.p++;
            double v;
            const char *err = ps.value(v);
            if (err) {
                return err;
            }
            if (c == 'n') {
                continue;
            }
            if (out.words.count >= MAX_WORDS) {
                return "too many words";
            }
            out.words.words[out.words.count++] = {(char)(c - 'a' + 'A'), v};
        }
        return nullptr;
    }

    bool scanOword(const char *text, OKind &kind, char (&label)[LABEL_MAX + 1])
    {
        char buf[MAX_LINE_LEN];
        if (!prepare(text, buf, nullptr)) {
            return false;
        }
        const char *p = skipLineNumber(buf);
        return *p == 'o' && !owordHead(p, kind, label);
    }

    bool routineLabel(const char *name, size_t len, char (&label)[LABEL_MAX + 1])
    {
        size_t n = 0;
        for (size_t i = 0; i < len; i++) {
            if (isSpace(name[i])) {
                continue;
            }
            if (n >= LABEL_MAX || name[i] == '<' || name[i] == '>') {
                return false;
            }
            label[n++] = lower(name[i]);
        }
        label[n] = '\0';
        return n > 0;
    }

    const char *applyAssigns(const Line &line, Params &params, ParamSource *source)
    {
        for (size_t i = 0; i < line.assignCount; i++) {
            const Assign &a = line.assigns[i];
            const char *err;
            if (a.number) {
                err = params.setNumbered(a.number, a.value);
            } else {
                double v;
                bool length;
                bool live;
                if (a.name[0] == '_' && source && source->get(a.name, v, length, live)) {
                    return "read-only parameter";
                }
                err = params.setNamed(a.name, a.value);
            }
            if (err) {
                return err;
            }
        }
        return nullptr;
    }

    const char *formatPrint(const char *message, Context &ctx, char *out, size_t outMax)
    {
        size_t n = 0;
        for (const char *p = message; *p && n + 1 < outMax;) {
            if (*p != '#') {
                out[n++] = *p++;
                continue;
            }
            char ref[LABEL_MAX + 3];
            size_t len = 0;
            const char *s = p + 1;
            if (*s == '<') {
                while (*s && *s != '>' && len + 2 < sizeof(ref)) {
                    if (!isSpace(*s)) {
                        ref[len++] = lower(*s);
                    }
                    s++;
                }
                if (*s == '>') {
                    ref[len++] = *s++;
                }
            } else {
                while (isDigit(*s) && len + 1 < sizeof(ref)) {
                    ref[len++] = *s++;
                }
            }
            ref[len] = '\0';
            double v = 0;
            Parser ps{ref, ctx, 0};
            int32_t number = 0;
            char name[LABEL_MAX + 1];
            if (!len || ps.reference(number, name) || *ps.p) {
                out[n++] = *p++;
                continue;
            }
            const char *readErr = ps.read(number, name, v);
            if (readErr) {
                return readErr;
            }
            int w = snprintf(out + n, outMax - n, "%.6f", v);
            n = w > 0 && (size_t)w < outMax - n ? n + (size_t)w : outMax - 1;
            p = s;
        }
        out[n] = '\0';
        return nullptr;
    }
}
