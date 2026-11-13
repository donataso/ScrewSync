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
#include "WebText.hpp"

using WebHttp::JsonWriter;
using WebHttp::urlQuery;
using WebHttp::urlQueryValue;

namespace
{
    char g_buf[128];

    void expect(const JsonWriter &json, const char *text)
    {
        zassert_true(json.ok);
        zassert_equal(json.pos, strlen(text), "length %zu", json.pos);
        zassert_mem_equal(json.buf, text, json.pos);
    }
}

ZTEST_SUITE(web_text, NULL, NULL, NULL, NULL, NULL);

ZTEST(web_text, test_query_value)
{
    char v[8];
    zassert_true(urlQuery("/gcode?id=abc&scale=1.5", "id", v, sizeof(v)));
    zassert_str_equal(v, "abc");
    zassert_true(urlQuery("/gcode?id=abc&scale=1.5", "scale", v, sizeof(v)));
    zassert_str_equal(v, "1.5");
    zassert_true(urlQuery("/gcode?id=", "id", v, sizeof(v)));
    zassert_str_equal(v, "");
    zassert_str_equal(urlQueryValue("/gcode?id=abc&scale=1.5", "id"), "abc&scale=1.5");
}

ZTEST(web_text, test_query_absent)
{
    char v[8] = "old";
    zassert_false(urlQuery("/gcode", "id", v, sizeof(v)));
    zassert_str_equal(v, "");
    zassert_false(urlQuery("/gcode?idx=1", "id", v, sizeof(v)));
    zassert_false(urlQuery("/gcode?xid=1", "id", v, sizeof(v)));
    zassert_false(urlQuery("/gcode?a=1&b=2", "id", v, sizeof(v)));
    zassert_is_null(urlQueryValue("/id=1", "id"));
}

ZTEST(web_text, test_query_too_long)
{
    char v[4];
    zassert_true(urlQuery("/x?id=abc", "id", v, sizeof(v)));
    zassert_false(urlQuery("/x?id=abcd", "id", v, sizeof(v)));
    zassert_str_equal(v, "");
}

ZTEST(web_text, test_json_raw_and_fmt)
{
    JsonWriter json(g_buf, sizeof(g_buf));
    json.raw("{\"n\":").fmt("%d", -42).raw(",\"f\":").fmt("%.2f", 1.5).raw("}");
    expect(json, "{\"n\":-42,\"f\":1.50}");
}

ZTEST(web_text, test_json_string_escapes)
{
    JsonWriter json(g_buf, sizeof(g_buf));
    json.str("a\"b\\c\n\r\t\x01z");
    expect(json, "\"a\\\"b\\\\c\\n\\r\\t\\u0001z\"");
}

ZTEST(web_text, test_json_string_or_null)
{
    JsonWriter json(g_buf, sizeof(g_buf));
    json.strOrNull("").raw(",").strOrNull("x");
    expect(json, "null,\"x\"");
}

ZTEST(web_text, test_json_overflow_is_sticky)
{
    char small[8];
    JsonWriter json(small, sizeof(small));
    json.raw("1234567");
    zassert_true(json.ok);
    json.raw("8");
    zassert_false(json.ok);
    zassert_equal(json.pos, 7);
    json.raw("");
    zassert_false(json.ok);
}

ZTEST(web_text, test_json_string_overflow)
{
    char small[8];
    JsonWriter json(small, sizeof(small));
    json.str("too long for it");
    zassert_false(json.ok);
    zassert_true(json.pos < sizeof(small));
}

ZTEST(web_text, test_json_fmt_too_long)
{
    JsonWriter json(g_buf, sizeof(g_buf));
    json.fmt("%0100d", 1);
    zassert_false(json.ok);
    zassert_equal(json.pos, 0);
}
