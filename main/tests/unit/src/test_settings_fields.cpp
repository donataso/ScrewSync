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
#include <math.h>
#include <string.h>
#include "SettingsFields.hpp"

using namespace Settings;

namespace
{
    Conf g_conf;
    char g_reason[64];

    bool set(const char *name, const char *value)
    {
        const Field *f = findField(name);
        zassert_not_null(f, "no field %s", name);
        g_reason[0] = '\0';
        return parseField(*f, value, g_conf, g_reason, sizeof(g_reason));
    }

    void reset(void *)
    {
        g_conf = Conf();
    }
}

ZTEST_SUITE(settings_fields, NULL, NULL, reset, NULL, NULL);

ZTEST(settings_fields, test_defaults_are_valid)
{
    zassert_true(validate(g_conf, g_reason, sizeof(g_reason)), "%s", g_reason);
}

ZTEST(settings_fields, test_fields_are_distinct)
{
    for (size_t i = 0; i < CONF_JSON_FIELDS; i++) {
        const Field &f = FIELDS[i];
        zassert_equal(findField(f.name), &f, "name %s twice", f.name);
        zassert_true(f.offset + fieldSize(f) <= sizeof(Conf));
        zassert_true(f.type == FieldType::Bool || f.min <= f.max);
        for (size_t k = 0; k < i; k++) {
            zassert_not_equal(FIELDS[k].offset, f.offset, "%s shares a field with %s", f.name, FIELDS[k].name);
        }
    }
    zassert_is_null(findField("nope"));
}

// setAllFromJson() pairs CONF_JSON_DESCR bits with FIELDS by index.
ZTEST(settings_fields, test_fields_follow_the_json_order)
{
    for (size_t i = 0; i < CONF_JSON_FIELDS; i++) {
        zassert_str_equal(FIELDS[i].name, CONF_JSON_DESCR[i].field_name);
        zassert_equal(FIELDS[i].offset, CONF_JSON_DESCR[i].offset);
    }
}

ZTEST(settings_fields, test_int)
{
    zassert_true(set("encoderPpr", "600"));
    zassert_equal(g_conf.encoderPpr, 600);
    zassert_true(set("encoderPpr", "15000"));
    zassert_true(set("encoderBacklash", "0"));
    zassert_equal(g_conf.encoderBacklash, 0);
}

ZTEST(settings_fields, test_int_refusals_keep_the_value)
{
    zassert_false(set("encoderPpr", "0"));
    zassert_str_equal(g_reason, "range [1, 15000]");
    zassert_false(set("encoderPpr", "15001"));
    zassert_false(set("encoderPpr", "12x"));
    zassert_false(set("encoderPpr", ""));
    zassert_false(set("encoderPpr", "1.5"));
    zassert_equal(g_conf.encoderPpr, Conf().encoderPpr);
}

ZTEST(settings_fields, test_bool)
{
    for (const char *on : {"1", "true", "on"}) {
        g_conf.invertEncoder = false;
        zassert_true(set("invertEncoder", on));
        zassert_true(g_conf.invertEncoder);
    }
    for (const char *off : {"0", "false", "off"}) {
        g_conf.invertEncoder = true;
        zassert_true(set("invertEncoder", off));
        zassert_false(g_conf.invertEncoder);
    }
    zassert_false(set("invertEncoder", "yes"));
    zassert_str_equal(g_reason, "expected 0|1|true|false|on|off");
}

ZTEST(settings_fields, test_float)
{
    zassert_true(set("handwheelPprZ", "100.5"));
    zassert_within(g_conf.z.handwheelPpr, 100.5f, 1e-6f);
    zassert_false(set("handwheelPprZ", "0.5"));
    zassert_false(set("handwheelPprZ", "abc"));
    zassert_within(g_conf.z.handwheelPpr, 100.5f, 1e-6f);
}

ZTEST(settings_fields, test_axis_fields_reach_their_axis)
{
    Conf defaults;
    zassert_true(set("motorStepsX", "400"));
    zassert_equal(g_conf.x.motorSteps, 400);
    zassert_equal(g_conf.z.motorSteps, defaults.z.motorSteps);
    zassert_true(set("invertDirectionZ", "0"));
    zassert_false(g_conf.z.invertDirection);
    zassert_equal(g_conf.x.invertDirection, defaults.x.invertDirection);
    zassert_true(set("backlashDuZ", "250"));
    zassert_equal(g_conf.z.backlashDu, 250);
    zassert_equal(g_conf.x.backlashDu, defaults.x.backlashDu);
}

ZTEST(settings_fields, test_validate_names_the_field)
{
    g_conf.z.motorSteps = 0;
    zassert_false(validate(g_conf, g_reason, sizeof(g_reason)));
    zassert_str_equal(g_reason, "motorStepsZ: range [1, 1000000]");
}

ZTEST(settings_fields, test_validate_refuses_bad_floats)
{
    g_conf.x.handwheelPpr = 0;
    zassert_false(validate(g_conf, g_reason, sizeof(g_reason)));
    g_conf.x.handwheelPpr = NAN;
    zassert_false(validate(g_conf, g_reason, sizeof(g_reason)));
    zassert_true(strncmp(g_reason, "handwheelPprX:", 14) == 0);
}

ZTEST(settings_fields, test_format_all)
{
    static char out[1536];
    formatAll(g_conf, out, sizeof(out));
    zassert_true(strncmp(out, "{\"encoderPpr\":1200,", 19) == 0, "%s", out);
    zassert_equal(out[strlen(out) - 1], '}');
    zassert_not_null(strstr(out, "\"invertDirectionZ\":true,"));
    zassert_not_null(strstr(out, "\"handwheelPprX\":600,"));
    for (const Field &f : FIELDS) {
        zassert_not_null(strstr(out, f.name));
    }
}

ZTEST(settings_fields, test_format_all_into_a_short_buffer)
{
    char out[24];
    memset(out, 'x', sizeof(out));
    formatAll(g_conf, out, sizeof(out));
    zassert_true(strlen(out) < sizeof(out));
}
