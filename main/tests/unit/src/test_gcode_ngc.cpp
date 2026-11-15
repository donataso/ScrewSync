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
#include <zephyr/ztest.h>
#include <string.h>
#include "GcodeDialects.hpp"
#include "GcodeMeta.hpp"
#include "GcodeProgram.hpp"

using namespace Gcode;

namespace
{
    Params g_params;
    Line g_line;

    struct Source : ParamSource
    {
        double contact = 0;
        bool get(const char *name, double &value, bool &length, bool &live) override
        {
            length = false;
            live = false;
            if (!strcmp(name, "_probe_feed")) {
                value = 30 * DU_PER_MM;
                length = true;
                return true;
            }
            if (!strcmp(name, "_probe_contact")) {
                value = contact;
                live = true;
                return true;
            }
            return false;
        }
    };

    struct Routines : RoutineSource
    {
        const char *find(const char *label, size_t &len) override
        {
            static const char *const BACK = "o<back> sub\nG91 G0 Z#1\nG90\no<back> endsub\n";
            if (strcmp(label, "back") != 0) {
                return nullptr;
            }
            len = strlen(BACK);
            return BACK;
        }
    };

    struct Collect : StepSink
    {
        Step steps[32];
        size_t count = 0;
        void emit(const Step &step) override
        {
            if (count < 32) {
                steps[count++] = step;
            }
        }
    };

    double eval(const char *expr)
    {
        char text[MAX_LINE_LEN];
        snprintf(text, sizeof(text), "#<_r> = %s", expr);
        Context ctx{&g_params, nullptr, DU_PER_MM};
        zassert_is_null(parseLine(text, ctx, g_line), "%s", expr);
        zassert_is_null(applyAssigns(g_line, g_params, nullptr));
        double v = 0;
        zassert_true(g_params.named("_r", v));
        return v;
    }

    const char *evalError(const char *expr)
    {
        char text[MAX_LINE_LEN];
        snprintf(text, sizeof(text), "#<_r> = %s", expr);
        Context ctx{&g_params, nullptr, DU_PER_MM};
        return parseLine(text, ctx, g_line);
    }

    // Runs to the end; probe steps trip at their target.
    const char *run(const char *text, Collect &sink, Source *source = nullptr)
    {
        static Program program;
        static Routines routines;
        g_params.clear();
        program.begin({find("linuxcnc"), &g_params, source, &routines, false}, text, strlen(text), 0, 0);
        while (true) {
            size_t before = sink.count;
            Program::Status s = program.next(sink, 1000);
            if (s == Program::Status::Error) {
                return program.error();
            }
            if (s == Program::Status::End) {
                return nullptr;
            }
            if (sink.count > before && sink.steps[sink.count - 1].kind == StepKind::Probe) {
                const Step &p = sink.steps[sink.count - 1];
                program.probeDone(p.zDu, p.xDu, p.zDu, p.xDu, true, p.zDu);
            }
        }
    }
}

ZTEST_SUITE(gcode_ngc, nullptr, nullptr, nullptr, nullptr, nullptr);

ZTEST(gcode_ngc, test_expressions)
{
    zassert_within(eval("[1 + 2 * 3]"), 7.0, 1e-9);
    zassert_within(eval("[[1 + 2] * 3]"), 9.0, 1e-9);
    zassert_within(eval("[2 ** 3 * 2]"), 16.0, 1e-9);
    zassert_within(eval("[7 MOD 3]"), 1.0, 1e-9);
    zassert_within(eval("[-7 mod 3]"), 2.0, 1e-9);
    zassert_within(eval("[1 + 1 EQ 2]"), 1.0, 1e-9);
    zassert_within(eval("[1 GT 2 OR 3 LT 4]"), 1.0, 1e-9);
    zassert_within(eval("[1 AND 0]"), 0.0, 1e-9);
    zassert_within(eval("[1 XOR 0]"), 1.0, 1e-9);
    zassert_within(eval("SIN[30]"), 0.5, 1e-9);
    zassert_within(eval("ATAN[1]/[1]"), 45.0, 1e-9);
    zassert_within(eval("[SQRT[16] + ABS[-2]]"), 6.0, 1e-9);
    zassert_within(eval("FIX[-1.5]"), -2.0, 1e-9);
    zassert_within(eval("FUP[1.2]"), 2.0, 1e-9);
    zassert_within(eval("ROUND[2.5]"), 3.0, 1e-9);
    zassert_within(eval("-[2]"), -2.0, 1e-9);
    zassert_not_null(evalError("[1 / 0]"));
    zassert_not_null(evalError("SQRT[-1]"));
    zassert_not_null(evalError("[1 + 2"));
    zassert_not_null(evalError("FOO[1]"));
}

ZTEST(gcode_ngc, test_parameters)
{
    g_params.clear();
    zassert_within(eval("5"), 5.0, 1e-9);
    Context ctx{&g_params, nullptr, DU_PER_MM};
    // Values on a line are read before its assignments apply.
    zassert_is_null(parseLine("#1 = 2 #2 = #1", ctx, g_line));
    zassert_is_null(applyAssigns(g_line, g_params, nullptr));
    double v;
    zassert_is_null(g_params.numbered(2, v));
    zassert_within(v, 0.0, 1e-9);
    zassert_is_null(parseLine("#<My Value> = #[1 + 0]", ctx, g_line));
    zassert_is_null(applyAssigns(g_line, g_params, nullptr));
    zassert_true(g_params.named("myvalue", v));
    zassert_within(v, 2.0, 1e-9);
    zassert_not_null(parseLine("G0 X#<nope>", ctx, g_line));
    zassert_not_null(parseLine("G0 X#31", ctx, g_line));
    zassert_is_null(parseLine("#5061 = 1", ctx, g_line));
    zassert_not_null(applyAssigns(g_line, g_params, nullptr));
}

ZTEST(gcode_ngc, test_source_in_program_units)
{
    Source source;
    Context ctx{&g_params, &source, DU_PER_INCH};
    zassert_is_null(parseLine("G1 F#<_probe_feed>", ctx, g_line));
    zassert_within(g_line.words.words[1].value, 30.0 / 25.4, 1e-9);
    zassert_is_null(parseLine("#<_probe_feed> = 1", ctx, g_line));
    zassert_not_null(applyAssigns(g_line, g_params, &source));
}

ZTEST(gcode_ngc, test_comments)
{
    Context ctx{&g_params, nullptr, DU_PER_MM};
    zassert_is_null(parseLine("G0 X1 (MSG, back off) (msg, second)", ctx, g_line));
    zassert_equal(g_line.comment, Comment::Msg);
    zassert_str_equal(g_line.message, "back off");
    zassert_equal(g_line.words.count, 2);
    zassert_is_null(parseLine("(abort,  no contact )", ctx, g_line));
    zassert_equal(g_line.comment, Comment::Abort);
    zassert_str_equal(g_line.message, "no contact");
}

ZTEST(gcode_ngc, test_oword_parse)
{
    Context ctx{&g_params, nullptr, DU_PER_MM};
    zassert_is_null(parseLine("O<Probe Z> call [1] [-1 * 2]", ctx, g_line));
    zassert_equal(g_line.okind, OKind::Call);
    zassert_str_equal(g_line.label, "probez");
    zassert_equal(g_line.argCount, 2);
    zassert_within(g_line.args[1], -2.0, 1e-9);
    zassert_is_null(parseLine("N10 o100 if [1]", ctx, g_line));
    zassert_equal(g_line.okind, OKind::If);
    zassert_str_equal(g_line.label, "100");
    zassert_not_null(parseLine("o100 if", ctx, g_line));
    zassert_not_null(parseLine("o100 elseif [1]", ctx, g_line));
    zassert_not_null(parseLine("G0 o100 endif", ctx, g_line));
    OKind kind;
    char label[LABEL_MAX + 1];
    zassert_true(scanOword("o101 endwhile (done)", kind, label));
    zassert_equal(kind, OKind::Endwhile);
    zassert_false(scanOword("G0 X1", kind, label));
}

ZTEST(gcode_ngc, test_flow)
{
    Collect sink;
    const char *text = "#1 = 0\n"
                       "o1 while [#1 LT 3]\n"
                       "  o2 if [#1 EQ 1]\n"
                       "    G0 Z1\n"
                       "  o2 else\n"
                       "    G0 Z2\n"
                       "  o2 endif\n"
                       "  #1 = [#1 + 1]\n"
                       "o1 endwhile\n"
                       "o3 repeat [2]\n"
                       "  #1 = [#1 + 1]\n"
                       "  G0 X#1\n"
                       "o3 endrepeat\n"
                       "o4 repeat [0]\n"
                       "  G0 X9\n"
                       "o4 endrepeat\n";
    zassert_is_null(run(text, sink));
    zassert_equal(sink.count, 5);
    zassert_equal(sink.steps[0].zDu, 2 * DU_PER_MM);
    zassert_equal(sink.steps[1].zDu, DU_PER_MM);
    zassert_equal(sink.steps[1].line, 4);
    zassert_equal(sink.steps[2].zDu, 2 * DU_PER_MM);
    zassert_equal(sink.steps[3].xDu, 4 * DU_PER_MM);
    zassert_equal(sink.steps[4].xDu, 5 * DU_PER_MM);
}

ZTEST(gcode_ngc, test_subs_and_routines)
{
    Collect sink;
    const char *text = "o<twice> sub\n"
                       "  G0 Z[#1 * 2]\n"
                       "  o<back> call [-1]\n"
                       "o<twice> endsub\n"
                       "G0 X1\n"
                       "o<twice> call [3]\n"
                       "G0 X#1\n";
    zassert_is_null(run(text, sink));
    zassert_equal(sink.count, 4);
    zassert_equal(sink.steps[1].zDu, 6 * DU_PER_MM);
    zassert_equal(sink.steps[2].zDu, 5 * DU_PER_MM);
    zassert_equal(sink.steps[2].line, 2);
    // Locals of the sub are gone after it returns.
    zassert_equal(sink.steps[3].xDu, 0);
    zassert_not_null(run("o<none> call\n", sink));
}

ZTEST(gcode_ngc, test_probe_results)
{
    Collect sink;
    const char *text = "G38.2 Z-4 F10\n"
                       "G0 X[#5063 * 2] Z#5070\n";
    zassert_is_null(run(text, sink));
    zassert_equal(sink.steps[0].kind, StepKind::Probe);
    zassert_equal(sink.steps[1].xDu, -8 * DU_PER_MM);
    zassert_equal(sink.steps[1].zDu, DU_PER_MM);
}

ZTEST(gcode_ngc, test_abort)
{
    Collect sink;
    Source source;
    source.contact = 1;
    const char *err = run("o1 if [#<_probe_contact>]\n(ABORT, contact not lost)\no1 endif\nG0 X1\n", sink, &source);
    zassert_not_null(err);
    zassert_str_equal(err, "contact not lost");
    zassert_equal(sink.count, 0);
}

ZTEST(gcode_ngc, test_check_program)
{
    uint32_t line;
    const char *ok = "o<a> sub\no1 if [1]\no1 else\no1 endif\no<a> endsub\no<a> call\n";
    zassert_is_null(checkProgram(ok, strlen(ok), line));
    const char *open = "G0 X1\no1 while [1]\nG0 X2\n";
    zassert_not_null(checkProgram(open, strlen(open), line));
    zassert_equal(line, 2);
    const char *crossed = "o1 if [1]\no2 while [1]\no1 endif\no2 endwhile\n";
    zassert_not_null(checkProgram(crossed, strlen(crossed), line));
    zassert_equal(line, 3);
    const char *nested = "o1 if [1]\no<a> sub\no<a> endsub\no1 endif\n";
    zassert_not_null(checkProgram(nested, strlen(nested), line));
    const char *bad = "G0 X1\nG0 X[1 +\n";
    zassert_not_null(checkProgram(bad, strlen(bad), line));
    zassert_equal(line, 2);
}

ZTEST(gcode_ngc, test_validate_counts)
{
    Dialect &dialect = *find("linuxcnc");
    const char *loop = "o1 repeat [3]\nG0 X1\nG0 X0\no1 endrepeat\n";
    GcodeMeta::ValidateResult r = GcodeMeta::validateProgram(loop, strlen(loop), dialect);
    zassert_true(r.ok);
    zassert_true(r.stepsKnown);
    zassert_equal(r.steps, 6);

    const char *probe = "G0 X1\nG38.2 Z-1 F10\nG0 X#5061\n";
    r = GcodeMeta::validateProgram(probe, strlen(probe), dialect);
    zassert_true(r.ok);
    zassert_false(r.stepsKnown);

    const char *call = "G0 X1\no<elsewhere> call\n";
    r = GcodeMeta::validateProgram(call, strlen(call), dialect);
    zassert_true(r.ok);
    zassert_false(r.stepsKnown);

    const char *routine = "o<r> sub\nG0 X1\no<r> endsub\n";
    r = GcodeMeta::validateProgram(routine, strlen(routine), *find(ROUTINE_DIALECT));
    zassert_true(r.ok);
    zassert_false(r.stepsKnown);
}

namespace
{
    const char PROBE_NGC[] = {
#include "routine_probe.inc"
    };

    struct ProbeSettings : ParamSource
    {
        bool get(const char *name, double &value, bool &length, bool &live) override
        {
            struct Entry
            {
                const char *name;
                double du;
            };
            static const Entry ENTRIES[] = {
                {"_probe_dst", 10 * DU_PER_MM},
                {"_probe_back_off", DU_PER_MM},
                {"_probe_feed", 30 * DU_PER_MM},
                {"_reprobe_feed", 10 * DU_PER_MM},
                {"_probe_offset", 2 * DU_PER_MM},
            };
            live = false;
            length = true;
            for (const Entry &e : ENTRIES) {
                if (!strcmp(name, e.name)) {
                    value = e.du;
                    return true;
                }
            }
            if (!strcmp(name, "_probe_contact")) {
                value = 0;
                length = false;
                live = true;
                return true;
            }
            return false;
        }
    };

    struct BuiltIn : RoutineSource
    {
        const char *find(const char *label, size_t &len) override
        {
            len = sizeof(PROBE_NGC);
            return strcmp(label, "probe") != 0 ? nullptr : PROBE_NGC;
        }
    };
}

ZTEST(gcode_ngc, test_builtin_probe)
{
    uint32_t line = 0;
    zassert_is_null(checkProgram(PROBE_NGC, sizeof(PROBE_NGC), line), "line %u", line);

    static Program program;
    ProbeSettings settings;
    BuiltIn routines;
    Collect sink;
    const char *call = "o<probe> call [0] [-1]";
    g_params.clear();
    program.begin({find("linuxcnc"), &g_params, &settings, &routines, false}, call, strlen(call), 0, 0);
    // Each touch trips 3 mm short of its target.
    while (true) {
        size_t before = sink.count;
        Program::Status s = program.next(sink, 1000);
        zassert_not_equal(s, Program::Status::Error, "%s line %u: %s", program.errorRoutine(), program.errorLine(),
                          program.error() ? program.error() : "");
        if (s == Program::Status::End) {
            break;
        }
        Step &last = sink.steps[sink.count - 1];
        if (sink.count > before && last.kind == StepKind::Probe) {
            int32_t trip = last.zDu + 3 * DU_PER_MM;
            program.probeDone(trip, 0, trip, 0, true, trip);
            last.zDu = trip;
        }
    }
    zassert_equal(sink.count, 5);
    zassert_equal(sink.steps[0].kind, StepKind::Probe);
    zassert_equal(sink.steps[0].zDu, -7 * DU_PER_MM);
    zassert_equal(sink.steps[0].feed, 30 * DU_PER_MM);
    zassert_equal(sink.steps[1].kind, StepKind::Feed);
    zassert_equal(sink.steps[1].zDu, -6 * DU_PER_MM);
    zassert_equal(sink.steps[1].xDu, 0);
    zassert_equal(sink.steps[2].kind, StepKind::Probe);
    zassert_equal(sink.steps[2].zDu, -6 * DU_PER_MM - 3 * DU_PER_MM / 2 + 3 * DU_PER_MM);
    zassert_equal(sink.steps[3].kind, StepKind::Feed);
    zassert_equal(sink.steps[4].kind, StepKind::SetPosition);
    zassert_equal(sink.steps[4].axes, AXIS_Z);
    zassert_equal(sink.steps[4].zDu, 3 * DU_PER_MM);
}

namespace
{
    const char ZERO_NGC[] = {
#include "routine_zero.inc"
    };
    const char BACKLASH_NGC[] = {
#include "routine_backlash.inc"
    };

    struct AllBuiltIns : RoutineSource
    {
        const char *find(const char *label, size_t &len) override
        {
            if (!strcmp(label, "zero")) {
                len = sizeof(ZERO_NGC);
                return ZERO_NGC;
            }
            if (!strcmp(label, "backlash")) {
                len = sizeof(BACKLASH_NGC);
                return BACKLASH_NGC;
            }
            return nullptr;
        }
    };

    struct Prints : PrintSink
    {
        char last[128] = "";
        void print(const char *text) override
        {
            strncpy(last, text, sizeof(last) - 1);
        }
    };
}

ZTEST(gcode_ngc, test_builtin_zero)
{
    uint32_t line = 0;
    zassert_is_null(checkProgram(ZERO_NGC, sizeof(ZERO_NGC), line), "line %u", line);
    static Program program;
    AllBuiltIns routines;
    Collect sink;
    const char *call = "o<zero> call [1] [1]";
    g_params.clear();
    program.begin({find("linuxcnc"), &g_params, nullptr, &routines, false}, call, strlen(call), 5 * DU_PER_MM,
                  2 * DU_PER_MM);
    while (program.next(sink, 1000) == Program::Status::Steps) {
    }
    zassert_equal(sink.count, 2);
    zassert_equal(sink.steps[0].zDu, 0);
    zassert_equal(sink.steps[0].xDu, 2 * DU_PER_MM);
    zassert_equal(sink.steps[1].xDu, 0);
    zassert_str_equal(program.message(), "position");
}

ZTEST(gcode_ngc, test_builtin_backlash)
{
    uint32_t line = 0;
    zassert_is_null(checkProgram(BACKLASH_NGC, sizeof(BACKLASH_NGC), line), "line %u", line);
    static Program program;
    ProbeSettings settings;
    AllBuiltIns routines;
    Prints prints;
    Collect sink;
    const char *call = "o<backlash> call [1] [-1] [2]";
    g_params.clear();
    Program::Setup setup = {find("linuxcnc"), &g_params, &settings, &routines, false, &prints};
    program.begin(setup, call, strlen(call), 0, 0);
    // Touches trip at their target. The motor travels 0.02 mm, then 0.04 mm,
    // more than the work position to open the contact.
    int32_t motor = 0;
    size_t releases = 0;
    while (true) {
        size_t before = sink.count;
        Program::Status s = program.next(sink, 1000);
        zassert_not_equal(s, Program::Status::Error, "%s line %u: %s", program.errorRoutine(), program.errorLine(),
                          program.error() ? program.error() : "");
        if (s == Program::Status::End) {
            break;
        }
        const Step &last = sink.steps[sink.count - 1];
        if (sink.count > before && last.kind == StepKind::Probe) {
            if (last.probe & PROBE_AWAY) {
                releases++;
                motor += (int32_t)(releases * 200);
            } else {
                motor = last.xDu;
            }
            program.probeDone(last.zDu, last.xDu, last.zDu, last.xDu, true, motor);
        }
    }
    zassert_equal(sink.count, 6);
    zassert_equal(sink.steps[0].xDu, -10 * DU_PER_MM);
    zassert_equal(sink.steps[1].probe, PROBE_AWAY);
    zassert_equal(sink.steps[1].xDu, -9 * DU_PER_MM);
    zassert_equal(sink.steps[3].xDu, -8 * DU_PER_MM - 3 * DU_PER_MM / 2);
    zassert_equal(sink.steps[3].feed, 10 * DU_PER_MM);
    zassert_str_equal(prints.last, "backlash 0.030000");
}
