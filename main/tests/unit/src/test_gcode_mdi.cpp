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

    Mdi session()
    {
        Mdi mdi;
        mdi.begin(*find("linuxcnc"), 0, 0);
        return mdi;
    }
}

ZTEST_SUITE(gcode_mdi, nullptr, nullptr, nullptr, nullptr, nullptr);

ZTEST(gcode_mdi, test_detection)
{
    for (const char *line : {"G0 Z1", "g91", "M100 Z1", "T1", "F100", "X-1", "Z.5", "z+1"}) {
        zassert_true(isMdiLine(line), "%s", line);
    }
    for (const char *line : {"", "G", "move z to 0", "zero z", "mode turn", "gear", "x", "ping", "@G0 Z1", " G0"}) {
        zassert_false(isMdiLine(line), "%s", line);
    }
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
