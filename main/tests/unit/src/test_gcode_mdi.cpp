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
#include <initializer_list>
#include <string.h>
#include "GcodeDialects.hpp"
#include "GcodeMdi.hpp"

using namespace Gcode;

namespace
{
    struct Collect : StepSink
    {
        Step steps[8];
        size_t count = 0;
        void emit(const Step &step) override
        {
            if (count < 8) {
                steps[count++] = step;
            }
        }
    };

    Params g_params;

    struct Printed : PrintSink
    {
        char text[4][64];
        size_t count = 0;
        void print(const char *line) override
        {
            if (count < 4) {
                strncpy(text[count], line, sizeof(text[count]) - 1);
                text[count][sizeof(text[count]) - 1] = '\0';
                count++;
            }
        }
    };

    Mdi session(PrintSink *printer = nullptr)
    {
        g_params.clear();
        Mdi mdi;
        mdi.begin(*find("linuxcnc"), 0, 0, g_params, nullptr, printer);
        return mdi;
    }

    const char *interpretLine(Interpreter &interp, const char *text, StepSink &sink)
    {
        static Line line;
        Context ctx{&g_params, nullptr, DU_PER_MM};
        const char *err = parseLine(text, ctx, line);
        return err ? err : interp.line(line.words, 1, sink);
    }
}

ZTEST_SUITE(gcode_mdi, nullptr, nullptr, nullptr, nullptr, nullptr);

ZTEST(gcode_mdi, test_detection)
{
    for (const char *line : {"G0 Z1", "g91", "M110", "T1", "F100", "X-1", "Z.5", "z+1", "#1 = 2", "#<depth> = 0.5", "(PRINT, hi)"}) {
        zassert_true(isMdiLine(line), "%s", line);
    }
    for (const char *line : {"", "G", "move z to 0", "zero z", "mode turn", "gear", "x", "ping", "@G0 Z1", " G0"}) {
        zassert_false(isMdiLine(line), "%s", line);
    }
}

ZTEST(gcode_mdi, test_print_comment_prints_with_parameters)
{
    Printed printed;
    Mdi mdi = session(&printed);
    Collect sink;
    zassert_is_null(mdi.line("#<d> = 0.5", false, 4, sink));
    zassert_equal(printed.count, 0);
    zassert_is_null(mdi.line("(PRINT, d #<d>)", false, 4, sink));
    zassert_equal(printed.count, 1);
    zassert_true(strncmp(printed.text[0], "d 0.5", 5) == 0, "%s", printed.text[0]);
}

ZTEST(gcode_mdi, test_print_not_repeated_for_refused_line)
{
    Printed printed;
    Mdi mdi = session(&printed);
    Collect sink;
    zassert_equal_ptr(mdi.line("G0 Z1 (PRINT, go)", false, 0, sink), "queue full");
    zassert_equal(printed.count, 0);
    zassert_is_null(mdi.line("G0 Z1 (PRINT, go)", false, 4, sink));
    zassert_equal(printed.count, 1);
}

ZTEST(gcode_mdi, test_print_takes_one_queue_slot)
{
    Printed printed;
    Mdi mdi = session(&printed);
    Collect sink;
    zassert_equal_ptr(mdi.line("(PRINT, x)", false, 0, sink), "queue full");
    zassert_is_null(mdi.line("(PRINT, x)", false, 1, sink));
    zassert_equal(sink.count, 1);
    zassert_equal(sink.steps[0].kind, StepKind::Print);
    zassert_equal_ptr(mdi.line("G0 Z1 (PRINT, y)", false, 1, sink), "queue full");
    zassert_is_null(mdi.line("G0 Z1 (PRINT, y)", false, 2, sink));
    zassert_equal(sink.count, 3);
    zassert_equal(sink.steps[1].kind, StepKind::Print);
    zassert_equal(sink.steps[2].kind, StepKind::Rapid);
}

ZTEST(gcode_mdi, test_print_unsupported_parameter_refuses_line)
{
    Printed printed;
    Mdi mdi = session(&printed);
    Collect sink;
    zassert_str_equal(mdi.line("G0 Z1 (PRINT, x #5241)", false, 4, sink), "unsupported parameter");
    zassert_str_equal(mdi.line("(PRINT, x #<nope>)", false, 4, sink), "undefined parameter");
    zassert_equal(sink.count, 0);
    zassert_equal(printed.count, 0);
    zassert_equal(mdi.zDu(), 0);
}

ZTEST(gcode_mdi, test_print_supported_parameters_format)
{
    Printed printed;
    Mdi mdi = session(&printed);
    Collect sink;
    zassert_is_null(mdi.line("#1 = 3", false, 4, sink));
    zassert_is_null(mdi.line("(PRINT, a #1)", false, 4, sink));
    zassert_true(strncmp(printed.text[0], "a 3", 3) == 0, "%s", printed.text[0]);
}

ZTEST(gcode_mdi, test_other_comments_do_not_print)
{
    Printed printed;
    Mdi mdi = session(&printed);
    Collect sink;
    zassert_is_null(mdi.line("G0 Z1 (MSG, hello)", false, 4, sink));
    zassert_is_null(mdi.line("(just a note)", false, 4, sink));
    zassert_equal(printed.count, 0);
}

ZTEST(gcode_mdi, test_modal_state_kept)
{
    Mdi mdi = session();
    Collect sink;
    zassert_is_null(mdi.line("G91 G1 F100", false, 4, sink));
    zassert_is_null(mdi.line("Z1", false, 4, sink));
    zassert_is_null(mdi.line("Z1", false, 4, sink));
    zassert_equal(sink.count, 2);
    zassert_equal(sink.steps[1].kind, StepKind::Feed);
    zassert_equal(sink.steps[1].zDu, 2 * DU_PER_MM);
    zassert_equal(sink.steps[1].feed, 100 * DU_PER_MM);
}

ZTEST(gcode_mdi, test_ui_line_keeps_modals)
{
    Mdi mdi = session();
    Collect sink;
    zassert_is_null(mdi.line("G91 G1 F100", false, 4, sink));
    zassert_is_null(mdi.line("G90 G0 Z5", true, 4, sink));
    zassert_is_null(mdi.line("Z1", false, 4, sink));
    zassert_equal(sink.count, 2);
    zassert_equal(sink.steps[0].kind, StepKind::Rapid);
    zassert_equal(sink.steps[1].kind, StepKind::Feed);
    zassert_equal(sink.steps[1].zDu, 6 * DU_PER_MM);
}

ZTEST(gcode_mdi, test_ui_line_starts_fresh)
{
    Mdi mdi = session();
    Collect sink;
    zassert_is_null(mdi.line("G1 F100", false, 4, sink));
    zassert_not_null(mdi.line("Z1", true, 4, sink));
}

ZTEST(gcode_mdi, test_set_position)
{
    Mdi mdi = session();
    Collect sink;
    mdi.setPosition(3 * DU_PER_MM, DU_PER_MM);
    zassert_is_null(mdi.line("G91 G0 Z1", false, 4, sink));
    zassert_equal(sink.steps[0].zDu, 4 * DU_PER_MM);
    zassert_equal(sink.steps[0].xDu, DU_PER_MM);
}

ZTEST(gcode_mdi, test_rejected_line_changes_nothing)
{
    Mdi mdi = session();
    Collect sink;
    zassert_is_null(mdi.line("G0 Z1", false, 4, sink));
    zassert_not_null(mdi.line("G91 G1 Z1", false, 4, sink));
    zassert_not_null(mdi.line("G91 Z1", false, 0, sink));
    zassert_is_null(mdi.line("Z2", false, 4, sink));
    zassert_equal(sink.count, 2);
    zassert_equal(sink.steps[1].kind, StepKind::Rapid);
    zassert_equal(sink.steps[1].zDu, 2 * DU_PER_MM);
}

ZTEST(gcode_mdi, test_program_codes_do_nothing)
{
    Mdi mdi = session();
    Collect sink;
    zassert_is_null(mdi.line("M0", false, 0, sink));
    zassert_is_null(mdi.line("M2", false, 0, sink));
    zassert_equal(sink.count, 0);
}

ZTEST(gcode_mdi, test_spindle_angle)
{
    Mdi mdi = session();
    Collect sink;
    zassert_is_null(mdi.line("M110 R90.5", false, 4, sink));
    zassert_is_null(mdi.line("M110", false, 4, sink));
    zassert_equal(sink.count, 2);
    zassert_equal(sink.steps[0].kind, StepKind::SpindleAngle);
    zassert_equal(sink.steps[0].feed, 90500);
    zassert_equal(sink.steps[1].feed, 0);
    zassert_not_null(mdi.line("G0 Z1 R5", false, 4, sink));
    zassert_not_null(mdi.line("G0 Z1 M110", false, 4, sink));
}

ZTEST(gcode_mdi, test_set_position_moves_session)
{
    Mdi mdi = session();
    Collect sink;
    mdi.setPosition(5 * DU_PER_MM, 0);
    zassert_is_null(mdi.line("G10 L20 P1 Z0", false, 4, sink));
    zassert_is_null(mdi.line("G91 G0 Z1", false, 4, sink));
    zassert_equal(sink.count, 2);
    zassert_equal(sink.steps[0].kind, StepKind::SetPosition);
    zassert_equal(sink.steps[0].axes, AXIS_Z);
    zassert_equal(sink.steps[1].zDu, DU_PER_MM);
}

ZTEST(gcode_mdi, test_wcs_kept_by_ui_line)
{
    Mdi mdi = session();
    Collect sink;
    WcsState wcs = {};
    wcs.offsetDu[1][0] = 2 * DU_PER_MM;
    mdi.setPosition(5 * DU_PER_MM, 0);
    mdi.setWcs(wcs);
    zassert_is_null(mdi.line("G55", true, 4, sink));
    zassert_equal(mdi.wcsActive(), 1);
    zassert_equal(mdi.zDu(), 3 * DU_PER_MM);
    zassert_is_null(mdi.line("G53 G0 Z0", false, 4, sink));
    zassert_equal(sink.steps[1].zDu, -2 * DU_PER_MM);
}

ZTEST(gcode_mdi, test_program_rejects_mdi_codes)
{
    Interpreter interp;
    find("linuxcnc")->begin(interp, 0, 0);
    Collect sink;
    zassert_not_null(interpretLine(interp, "M110", sink));
}

ZTEST(gcode_mdi, test_probe)
{
    Mdi mdi = session();
    Collect sink;
    zassert_is_null(mdi.line("G91 G38.2 Z-5 F100", false, 4, sink));
    zassert_is_null(mdi.line("G38.5 X1", false, 4, sink));
    zassert_equal(sink.count, 2);
    zassert_equal(sink.steps[0].kind, StepKind::Probe);
    zassert_equal(sink.steps[0].zDu, -5 * DU_PER_MM);
    zassert_equal(sink.steps[0].feed, 100 * DU_PER_MM);
    zassert_equal(sink.steps[0].probe, 0);
    zassert_equal(sink.steps[1].probe, PROBE_AWAY | PROBE_NO_ERROR);
    zassert_equal(sink.steps[1].xDu, DU_PER_MM);
    // Not modal.
    zassert_not_null(mdi.line("Z-1", false, 4, sink));
    zassert_not_null(mdi.line("G38.2 Z-1 X1", false, 4, sink));
    zassert_not_null(mdi.line("G38.2", false, 4, sink));
    zassert_not_null(mdi.line("G38.2 Z0", false, 4, sink));
    zassert_not_null(mdi.line("G0 G38.2 Z-1", false, 4, sink));
    zassert_not_null(mdi.line("G38.1 Z-1", false, 4, sink));
    zassert_not_null(mdi.line("G38.2 Z-1", true, 4, sink));
}

ZTEST(gcode_mdi, test_parameters)
{
    Mdi mdi = session();
    Collect sink;
    zassert_is_null(mdi.line("#<_d> = 2", false, 4, sink));
    zassert_is_null(mdi.line("G0 Z[#<_d> * 2]", false, 4, sink));
    zassert_equal(sink.count, 1);
    zassert_equal(sink.steps[0].zDu, 4 * DU_PER_MM);
    zassert_not_null(mdi.line("o<x> call", false, 4, sink));
    mdi.probeDone(-3 * DU_PER_MM, DU_PER_MM, true, -2 * DU_PER_MM);
    zassert_is_null(mdi.line("G0 Z#5063 X#5061", false, 4, sink));
    zassert_equal(sink.steps[1].zDu, -3 * DU_PER_MM);
    zassert_equal(sink.steps[1].xDu, DU_PER_MM);
    zassert_is_null(mdi.line("G0 Z#<_probe_motor>", false, 4, sink));
    zassert_equal(sink.steps[2].zDu, -2 * DU_PER_MM);
}
