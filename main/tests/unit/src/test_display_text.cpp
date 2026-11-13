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
#include "DisplayText.hpp"

using namespace Display::Detail;
using ModeCells::Cell;
using ModeCells::Kind;

namespace
{
    Cell cell(Kind kind, const char *when = nullptr)
    {
        return {kind, "p", "P", nullptr, nullptr, nullptr, nullptr, when};
    }

    const char *length(double du, bool inch)
    {
        static char out[24];
        formatLength(du, inch, out, sizeof(out));
        return out;
    }

    const char *trimmed(const char *text)
    {
        static char out[24];
        strcpy(out, text);
        trimZeros(out);
        return out;
    }

    const char *value(Kind kind, const char *raw, bool inch = false)
    {
        static char out[32];
        formatCellValue(cell(kind), raw, inch, out, sizeof(out));
        return out;
    }
}

ZTEST_SUITE(display_text, NULL, NULL, NULL, NULL, NULL);

ZTEST(display_text, test_format_length)
{
    zassert_str_equal(length(12340, false), "1.234");
    zassert_str_equal(length(-5000, false), "-0.500");
    zassert_str_equal(length(1234560, false), "123.456");
    zassert_str_equal(length(254000, true), "1.0000");
    zassert_str_equal(length(-127000, true), "-0.5000");
    zassert_str_equal(length(0, false), "0.000");
}

ZTEST(display_text, test_format_length_has_no_negative_zero)
{
    zassert_str_equal(length(-1, false), "0.000");
    zassert_str_equal(length(-1, true), "0.0000");
}

ZTEST(display_text, test_unit_suffix)
{
    zassert_str_equal(unitSuffix(false), "mm");
    zassert_str_equal(unitSuffix(true), "in");
}

ZTEST(display_text, test_trim_zeros)
{
    zassert_str_equal(trimmed("1.500"), "1.5");
    zassert_str_equal(trimmed("2.000"), "2");
    zassert_str_equal(trimmed("0.000"), "0");
    zassert_str_equal(trimmed("100"), "100");
    zassert_str_equal(trimmed("10.01"), "10.01");
}

ZTEST(display_text, test_param_value)
{
    const char *params = "pitch=15000 axis=z dir=- stop-after=0";
    char v[16];
    zassert_true(paramValue(params, "pitch", v, sizeof(v)));
    zassert_str_equal(v, "15000");
    zassert_true(paramValue(params, "axis", v, sizeof(v)));
    zassert_str_equal(v, "z");
    zassert_true(paramValue(params, "stop-after", v, sizeof(v)));
    zassert_str_equal(v, "0");
    zassert_false(paramValue(params, "pit", v, sizeof(v)));
    zassert_false(paramValue(params, "after", v, sizeof(v)));
    zassert_false(paramValue("", "pitch", v, sizeof(v)));
}

ZTEST(display_text, test_cell_shown)
{
    zassert_true(cellShown(cell(Kind::Angle), "axis=x"));
    zassert_true(cellShown(cell(Kind::Angle, "axis=z"), "pitch=1 axis=z"));
    zassert_false(cellShown(cell(Kind::Angle, "axis=z"), "pitch=1 axis=x"));
    zassert_false(cellShown(cell(Kind::Angle, "axis=z"), "pitch=1"));
}

ZTEST(display_text, test_cell_value_lengths)
{
    zassert_str_equal(value(Kind::Length, "15000"), "1.5 mm");
    zassert_str_equal(value(Kind::Pitch, "12500"), "1.25 mm");
    zassert_str_equal(value(Kind::Feed, "1000"), "0.1 mm/rev");
    zassert_str_equal(value(Kind::Speed, "3000000"), "300 mm/min");
    zassert_str_equal(value(Kind::Feed, "254000", true), "1 in/rev");
}

ZTEST(display_text, test_cell_value_reports_zero)
{
    char out[32];
    zassert_false(formatCellValue(cell(Kind::Length), "0", false, out, sizeof(out)));
    zassert_str_equal(out, "0 mm");
    zassert_true(formatCellValue(cell(Kind::Length), "1", false, out, sizeof(out)));
    zassert_true(formatCellValue(cell(Kind::Count), "0", false, out, sizeof(out)));
    zassert_true(formatCellValue(cell(Kind::Angle), "0", false, out, sizeof(out)));
}

ZTEST(display_text, test_cell_value_count_and_angle)
{
    zassert_str_equal(value(Kind::Count, "5"), "5");
    zassert_str_equal(value(Kind::Angle, "1.79"), "1.79°");
    zassert_str_equal(value(Kind::Angle, "30"), "30°");
    zassert_str_equal(value(Kind::Angle, "0"), "0°");
    zassert_str_equal(value(Kind::Angle, "-0.001"), "0°");
    zassert_str_equal(value(Kind::Angle, "-1.5"), "-1.5°");
}

ZTEST(display_text, test_group_label)
{
    char out[16];
    groupLabel("bsp", out, sizeof(out));
    zassert_str_equal(out, "BSP");
    groupLabel("unef", out, sizeof(out));
    zassert_str_equal(out, "UNEF");
    groupLabel("metric", out, sizeof(out));
    zassert_str_equal(out, "Metric");
    groupLabel("metric", out, 4);
    zassert_str_equal(out, "Met");
    groupLabel("", out, sizeof(out));
    zassert_str_equal(out, "");
}

ZTEST(display_text, test_designation_label)
{
    char out[32];
    designationLabel("M10x1.25", out, sizeof(out));
    zassert_str_equal(out, "M10×1.25");
    designationLabel("1/2NPT", out, sizeof(out));
    zassert_str_equal(out, "1/2NPT");
    designationLabel("x1", out, sizeof(out));
    zassert_str_equal(out, "x1");
    designationLabel("M10xL", out, sizeof(out));
    zassert_str_equal(out, "M10xL");
    designationLabel("M10x1.25", out, 6);
    zassert_str_equal(out, "M10");
}

ZTEST(display_text, test_list_items)
{
    const char *list = "metric,unc,bsp";
    char item[16];
    zassert_true(nextItem(list, item, sizeof(item)));
    zassert_str_equal(item, "metric");
    zassert_true(nextItem(list, item, sizeof(item)));
    zassert_str_equal(item, "unc");
    zassert_true(nextItem(list, item, sizeof(item)));
    zassert_str_equal(item, "bsp");
    zassert_false(nextItem(list, item, sizeof(item)));
    zassert_str_equal(item, "");
}

ZTEST(display_text, test_has_item)
{
    zassert_true(hasItem("metric,unc,bsp", "metric"));
    zassert_true(hasItem("metric,unc,bsp", "bsp"));
    zassert_false(hasItem("metric,unc,bsp", "un"));
    zassert_false(hasItem("metric,unc,bsp", "uncx"));
    zassert_false(hasItem("", "metric"));
}
