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
#include <stddef.h>
#include <string.h>
#include "ModeParamCodec.hpp"

using namespace Mode::Detail;

namespace
{
    struct Params
    {
        int32_t pitchDu;
        int32_t passes;
        Stepper::Axis axis;
        bool reverse;
        bool internal;
        float taper;
        char preset[8];
    };

    const Param TABLE[] = {
        {"pitch", Kind::Du, offsetof(Params, pitchDu), 0, 1000000, true, true},
        {"passes", Kind::Count, offsetof(Params, passes), 1, 1000, false},
        {"axis", Kind::Axis, offsetof(Params, axis), 0, 0, false},
        {"dir", Kind::Flag, offsetof(Params, reverse), 0, 0, false, false, "-|+"},
        {"internal", Kind::Flag, offsetof(Params, internal), 0, 0, false},
        {"taper", Kind::Angle, offsetof(Params, taper), -1, 1, false},
        {"preset", Kind::Text, offsetof(Params, preset), 0, sizeof(Params::preset), false},
    };
    constexpr size_t COUNT = sizeof(TABLE) / sizeof(TABLE[0]);

    Params g_params;

    uint8_t *raw()
    {
        return (uint8_t *)&g_params;
    }

    const char *set(const char *name, const char *value)
    {
        const Param *p = findParam(TABLE, COUNT, name);
        zassert_not_null(p);
        return parseParam(*p, value, raw());
    }

    const char *load(const char *line)
    {
        return loadParamLine(TABLE, COUNT, line, raw());
    }

    void reset(void *)
    {
        g_params = {15000, 5, Stepper::Axis::Z, false, false, 0, ""};
    }
}

ZTEST_SUITE(mode_param_codec, NULL, NULL, reset, NULL, NULL);

ZTEST(mode_param_codec, test_find_param)
{
    zassert_equal(findParam(TABLE, COUNT, "passes"), &TABLE[1]);
    zassert_is_null(findParam(TABLE, COUNT, "pass"));
    zassert_is_null(findParam(TABLE, COUNT, ""));
}

ZTEST(mode_param_codec, test_number)
{
    zassert_is_null(set("pitch", "12500"));
    zassert_equal(g_params.pitchDu, 12500);
    zassert_is_null(set("pitch", "off"));
    zassert_equal(g_params.pitchDu, 0);
    zassert_is_null(set("passes", "1000"));
    zassert_equal(g_params.passes, 1000);
}

ZTEST(mode_param_codec, test_number_refusals_keep_the_value)
{
    zassert_str_equal(set("pitch", "abc"), "not a number");
    zassert_str_equal(set("pitch", "12x"), "not a number");
    zassert_str_equal(set("pitch", ""), "not a number");
    zassert_str_equal(set("pitch", "-1"), "out of range");
    zassert_str_equal(set("pitch", "1000001"), "out of range");
    zassert_str_equal(set("passes", "0"), "out of range");
    // "off" is for lengths only.
    zassert_str_equal(set("passes", "off"), "not a number");
    zassert_equal(g_params.pitchDu, 15000);
    zassert_equal(g_params.passes, 5);
}

ZTEST(mode_param_codec, test_axis)
{
    zassert_is_null(set("axis", "x"));
    zassert_equal(g_params.axis, Stepper::Axis::X);
    zassert_is_null(set("axis", "z"));
    zassert_equal(g_params.axis, Stepper::Axis::Z);
    zassert_not_null(set("axis", "y"));
    zassert_equal(g_params.axis, Stepper::Axis::Z);
}

ZTEST(mode_param_codec, test_flag)
{
    zassert_is_null(set("internal", "on"));
    zassert_true(g_params.internal);
    zassert_is_null(set("internal", "off"));
    zassert_false(g_params.internal);
    zassert_is_null(set("internal", "1"));
    zassert_true(g_params.internal);
    zassert_is_null(set("internal", "0"));
    zassert_false(g_params.internal);
    zassert_str_equal(set("internal", "yes"), "off|on");
}

ZTEST(mode_param_codec, test_flag_with_choices)
{
    zassert_is_null(set("dir", "+"));
    zassert_true(g_params.reverse);
    zassert_is_null(set("dir", "-"));
    zassert_false(g_params.reverse);
    zassert_str_equal(set("dir", "on"), "-|+");
}

ZTEST(mode_param_codec, test_angle_is_stored_as_diameter_ratio)
{
    // NPT: 1.7899 degrees half-angle = 1:16 on diameter.
    zassert_is_null(set("taper", "1.7899"));
    zassert_within(g_params.taper, 0.0625f, 1e-5f);
    zassert_is_null(set("taper", "-1.7899"));
    zassert_within(g_params.taper, -0.0625f, 1e-5f);
    zassert_str_equal(set("taper", "45"), "out of range");
    zassert_str_equal(set("taper", "90"), "out of range");
    zassert_str_equal(set("taper", "x"), "not a number");
    zassert_within(g_params.taper, -0.0625f, 1e-5f);
}

ZTEST(mode_param_codec, test_text_is_read_only)
{
    zassert_str_equal(set("preset", "M10"), "read-only");
    zassert_str_equal(g_params.preset, "");
}

ZTEST(mode_param_codec, test_describe)
{
    char out[128];
    describeParams(TABLE, COUNT, raw(), out, sizeof(out));
    zassert_str_equal(out, "pitch=15000 passes=5 axis=z dir=- internal=off taper=0 preset=-");

    g_params = {12500, 8, Stepper::Axis::X, true, true, 0.0625f, "M8x1"};
    describeParams(TABLE, COUNT, raw(), out, sizeof(out));
    zassert_str_equal(out, "pitch=12500 passes=8 axis=x dir=+ internal=on taper=1.79 preset=M8x1");
}

ZTEST(mode_param_codec, test_describe_into_a_short_buffer)
{
    char out[12];
    describeParams(TABLE, COUNT, raw(), out, sizeof(out));
    zassert_str_equal(out, "pitch=15000");
}

ZTEST(mode_param_codec, test_load_line_restores_a_description)
{
    Params saved = {12500, 8, Stepper::Axis::X, true, true, 0.0625f, "M8x1"};
    g_params = saved;
    char line[128];
    describeParams(TABLE, COUNT, raw(), line, sizeof(line));

    reset(nullptr);
    zassert_is_null(load(line));
    zassert_equal(g_params.pitchDu, saved.pitchDu);
    zassert_equal(g_params.passes, saved.passes);
    zassert_equal(g_params.axis, saved.axis);
    zassert_equal(g_params.reverse, saved.reverse);
    zassert_equal(g_params.internal, saved.internal);
    zassert_str_equal(g_params.preset, saved.preset);

    char again[128];
    describeParams(TABLE, COUNT, raw(), again, sizeof(again));
    zassert_str_equal(again, line);
}

ZTEST(mode_param_codec, test_load_line_text)
{
    zassert_is_null(load("preset=M10"));
    zassert_str_equal(g_params.preset, "M10");
    zassert_is_null(load("preset=-"));
    zassert_str_equal(g_params.preset, "");
    zassert_str_equal(load("preset=12345678"), "too long");
    zassert_is_null(load("preset=1234567"));
}

ZTEST(mode_param_codec, test_load_line_spacing_and_subsets)
{
    zassert_is_null(load(""));
    zassert_is_null(load("  passes=9   axis=x "));
    zassert_equal(g_params.passes, 9);
    zassert_equal(g_params.axis, Stepper::Axis::X);
    zassert_equal(g_params.pitchDu, 15000);
}

ZTEST(mode_param_codec, test_load_line_refusals)
{
    zassert_str_equal(load("passes"), "expected name=value");
    zassert_str_equal(load("nope=1"), "unknown param");
    zassert_str_equal(load("passes=0"), "out of range");
    zassert_str_equal(load("passes=7 axis=q"), "axis must be z or x");
    char longToken[80];
    memset(longToken, 'a', sizeof(longToken) - 1);
    longToken[sizeof(longToken) - 1] = '\0';
    zassert_str_equal(load(longToken), "param too long");
}

ZTEST(mode_param_codec, test_required_params)
{
    zassert_is_null(firstUnsetParam(TABLE, COUNT, raw()));
    g_params.pitchDu = 0;
    zassert_equal(firstUnsetParam(TABLE, COUNT, raw()), &TABLE[0]);
    zassert_false(paramIsSet(TABLE[0], raw()));
    zassert_true(paramIsSet(TABLE[1], raw()));
}
