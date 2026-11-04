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
#include "GcodeMeta.hpp"
#include "GcodeLathecode.hpp"

using namespace GcodeMeta;

ZTEST_SUITE(gcode_meta, NULL, NULL, NULL, NULL, NULL);

ZTEST(gcode_meta, test_valid_name)
{
    zassert_true(validName("part1", 5));
    zassert_true(validName("caf\xc3\xa9", 5)); // UTF-8 "café"
    zassert_false(validName("", 0));
    zassert_false(validName(nullptr, 0));
    zassert_false(validName("x\ty", 3)); // tab is a control char
    zassert_false(validName("x\x7fy", 3)); // DEL
    char tooLong[NAME_MAX_LEN + 2];
    memset(tooLong, 'a', sizeof(tooLong));
    zassert_true(validName(tooLong, NAME_MAX_LEN));
    zassert_false(validName(tooLong, NAME_MAX_LEN + 1));
}

ZTEST(gcode_meta, test_header_round_trip)
{
    char buf[128];
    int n = encodeHeader("lathecode", "part1", buf, sizeof(buf));
    zassert_true(n > 0);
    zassert_mem_equal(buf, "lathecode\tpart1\n", n);

    Header h;
    zassert_true(decodeHeader(buf, n, h));
    zassert_equal(h.flavorLen, 9);
    zassert_mem_equal(h.flavor, "lathecode", h.flavorLen);
    zassert_equal(h.nameLen, 5);
    zassert_mem_equal(h.name, "part1", h.nameLen);
    zassert_equal(h.bodyOffset, (size_t)n);
}

ZTEST(gcode_meta, test_encode_header_too_small)
{
    char buf[4];
    zassert_equal(encodeHeader("lathecode", "part1", buf, sizeof(buf)), -1);
}

ZTEST(gcode_meta, test_decode_header_with_body)
{
    const char *text = "lathecode\tmy part\nG0 X0 Z0\nG1 X1\n";
    Header h;
    zassert_true(decodeHeader(text, strlen(text), h));
    zassert_equal(h.flavorLen, 9);
    zassert_equal(h.nameLen, 7);
    zassert_mem_equal(h.name, "my part", h.nameLen);
    zassert_str_equal(text + h.bodyOffset, "G0 X0 Z0\nG1 X1\n");
}

ZTEST(gcode_meta, test_decode_header_malformed)
{
    Header h;
    zassert_false(decodeHeader("no newline here", 16, h)); // no '\n'
    zassert_false(decodeHeader("noheadertab\n", 12, h)); // no tab
    zassert_false(decodeHeader("\tname\n", 6, h)); // empty flavor
    zassert_false(decodeHeader("lathecode\t\n", 11, h)); // empty name
}

ZTEST(gcode_meta, test_validate_program_ok)
{
    Gcode::Lathecode flavor;
    const char *text = "G0 Z-1 X0\nG1 X1 Z-2 F100\n";
    ValidateResult r = validateProgram(text, strlen(text), flavor);
    zassert_true(r.ok);
    zassert_is_null(r.error);
    zassert_equal(r.steps, 2);
}

ZTEST(gcode_meta, test_validate_program_axes_mismatch)
{
    Gcode::Lathecode flavor;
    const char *text = "; Axes expected: Z+ points left, X+ points up\nG1 X1 Z-2 F100\n";
    ValidateResult r = validateProgram(text, strlen(text), flavor);
    zassert_false(r.ok);
    zassert_not_null(strstr(r.error, "axes mismatch"));
}

ZTEST(gcode_meta, test_validate_program_empty)
{
    Gcode::Lathecode flavor;
    ValidateResult r = validateProgram("", 0, flavor);
    zassert_false(r.ok);
    zassert_str_equal(r.error, "empty program");
}

ZTEST(gcode_meta, test_validate_program_embedded_nul)
{
    Gcode::Lathecode flavor;
    char text[] = {'G', '0', ' ', 'X', '0', '\0', '\n'};
    ValidateResult r = validateProgram(text, sizeof(text), flavor);
    zassert_false(r.ok);
    zassert_str_equal(r.error, "embedded NUL byte");
}

ZTEST(gcode_meta, test_validate_program_line_too_long)
{
    Gcode::Lathecode flavor;
    char text[Gcode::MAX_LINE_LEN + 32];
    size_t n = 0;
    text[n++] = 'G';
    text[n++] = '0';
    text[n++] = ' ';
    text[n++] = 'X';
    while (n < sizeof(text) - 2) {
        text[n++] = '0';
    }
    text[n++] = '\n';
    ValidateResult r = validateProgram(text, n, flavor);
    zassert_false(r.ok);
    zassert_str_equal(r.error, "line too long");
    zassert_equal(r.line, 1);
}

ZTEST(gcode_meta, test_validate_program_syntax_error_reports_line)
{
    Gcode::Lathecode flavor;
    const char *text = "G0 X0 Z0\nG97\nG1 X1\n";
    ValidateResult r = validateProgram(text, strlen(text), flavor);
    zassert_false(r.ok);
    zassert_not_null(r.error);
    zassert_equal(r.line, 2);
}

ZTEST(gcode_meta, test_validate_program_no_motion_steps)
{
    Gcode::Lathecode flavor;
    const char *text = "G20\nG90\n(just comments)\n";
    ValidateResult r = validateProgram(text, strlen(text), flavor);
    zassert_false(r.ok);
    zassert_str_equal(r.error, "no motion steps");
}

ZTEST(gcode_meta, test_axes_mismatch_no_header)
{
    const char *text = "G0 X0 Z0\n";
    zassert_false(axesMismatch(text, strlen(text)));
}

ZTEST(gcode_meta, test_axes_mismatch_matches)
{
    const char *text = "; Axes expected: Z+ points right, X+ points down\nG0 X0 Z0\n";
    zassert_false(axesMismatch(text, strlen(text)));
}

ZTEST(gcode_meta, test_axes_mismatch_x_wrong)
{
    const char *text = "; Axes expected: Z+ points right, X+ points up\nG0 X0 Z0\n";
    zassert_true(axesMismatch(text, strlen(text)));
}

ZTEST(gcode_meta, test_axes_mismatch_z_wrong)
{
    const char *text = "; Axes expected: Z+ points left, X+ points down\nG0 X0 Z0\n";
    zassert_true(axesMismatch(text, strlen(text)));
}

ZTEST(gcode_meta, test_axes_mismatch_both_wrong)
{
    const char *text = "; Axes expected: Z+ points left, X+ points up\nG0 X0 Z0\n";
    zassert_true(axesMismatch(text, strlen(text)));
}

ZTEST(gcode_meta, test_axes_mismatch_case_insensitive)
{
    const char *text = "; AXES EXPECTED: z+ POINTS RIGHT, x+ POINTS DOWN\nG0 X0 Z0\n";
    zassert_false(axesMismatch(text, strlen(text)));
}
