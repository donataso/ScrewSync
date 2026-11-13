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
#include <zephyr/data/json.h>
#include <zephyr/sys/util.h>
#include <string.h>
#include "JsonSpan.hpp"
#include "Settings.hpp"

namespace
{
    char g_buf[4096];

    bool find(const char *json, const char *key, const char *expected)
    {
        strcpy(g_buf, json);
        char *start = nullptr;
        size_t len = 0;
        if (!JsonSpan::findObject(g_buf, strlen(g_buf), key, start, len)) {
            return expected == nullptr;
        }
        return expected && len == strlen(expected) && !memcmp(start, expected, len);
    }

    struct SettingsDoc
    {
        Settings::Conf settings;
    };

    const struct json_obj_descr SETTINGS_DOC_DESCR[] = {
        JSON_OBJ_DESCR_OBJECT(SettingsDoc, settings, Settings::CONF_JSON_DESCR),
    };

    SettingsDoc g_doc;
    Settings::Conf g_parsed;
    char g_reencoded[4096];
}

ZTEST(json_span, test_finds_top_level_object)
{
    zassert_true(find(R"({"a":1,"settings":{"x":[1,{"y":2}]},"b":2})", "settings", R"({"x":[1,{"y":2}]})"));
    zassert_true(find(R"({ "settings" : {} })", "settings", "{}"));
}

ZTEST(json_span, test_ignores_nested_and_string_matches)
{
    zassert_true(find(R"({"run":{"settings":{"a":1}}})", "settings", nullptr));
    zassert_true(find(R"({"mode":"settings","x":{}})", "settings", nullptr));
    zassert_true(find(R"({"s":"\"settings\":{","settings":{"q":"}"}})", "settings", R"({"q":"}"})"));
}

ZTEST(json_span, test_rejects_missing_or_broken)
{
    zassert_true(find(R"({"settings":1})", "settings", nullptr));
    zassert_true(find(R"({"settings":{"a":1)", "settings", nullptr));
    zassert_true(find(R"({"settings":{"a":"1)", "settings", nullptr));
}

// Every settings field, so the field mask passes bit 31.
ZTEST(json_span, test_full_settings_round_trip)
{
    g_doc.settings = Settings::Conf{};
    g_doc.settings.x.invertDirection = true;
    g_doc.settings.x.invertHandwheel = true;
    zassert_ok(json_obj_encode_buf(SETTINGS_DOC_DESCR, ARRAY_SIZE(SETTINGS_DOC_DESCR), &g_doc, g_buf, sizeof(g_buf)));
    char *start = nullptr;
    size_t len = 0;
    zassert_true(JsonSpan::findObject(g_buf, strlen(g_buf), "settings", start, len));
    int64_t mask = json_obj_parse(start, len, Settings::CONF_JSON_DESCR, Settings::CONF_JSON_FIELDS, &g_parsed);
    zassert_equal(mask, (int64_t)BIT64_MASK(Settings::CONF_JSON_FIELDS));

    g_doc.settings = g_parsed;
    zassert_ok(json_obj_encode_buf(SETTINGS_DOC_DESCR, ARRAY_SIZE(SETTINGS_DOC_DESCR), &g_doc, g_reencoded,
                                   sizeof(g_reencoded)));
    zassert_true(strstr(g_reencoded, "\"invertDirectionX\":true") != nullptr);
    zassert_true(strstr(g_reencoded, "\"invertHandwheelX\":true") != nullptr);
}

ZTEST_SUITE(json_span, NULL, NULL, NULL, NULL, NULL);
