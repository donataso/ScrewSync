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

ZTEST(settings_fields, test_handwheel_lag)
{
    zassert_true(set("handwheelLagMsX", "250"));
    zassert_equal(g_conf.x.handwheelLagMs, 250);
    zassert_false(set("handwheelLagMsX", "0"));
    zassert_equal(g_conf.x.handwheelLagMs, 250);
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

ZTEST(settings_fields, test_old_names_of_a_field)
{
    const Field &rapid = *findField("speedRapidDuPerMinX");
    zassert_str_equal(oldName(rapid, 0), "speedRapidX");
    zassert_str_equal(oldName(rapid, 1), "speedManualMoveX");
    zassert_is_null(oldName(rapid, 2));
    zassert_str_equal(oldName(*findField("decelerationDuPerSec2Z"), 0), "decelerationZ");
    zassert_is_null(oldName(*findField("encoderPpr"), 0));
    zassert_is_null(findField("speedManualMoveX"));
    zassert_is_null(findField("speedRapidZ"));
}

namespace
{
    int64_t parse(const char *json)
    {
        static char buf[256];
        strcpy(buf, json);
        return parseJson(buf, strlen(buf), g_conf);
    }

    size_t indexOf(const char *name)
    {
        return (size_t)(findField(name) - FIELDS);
    }
}

ZTEST(settings_fields, test_convert_legacy)
{
    g_conf.z.motorSteps = 1600;
    g_conf.z.screwPitchDu = 20000;
    g_conf.z.speedRapidDuPerMin = 12800;
    convertLegacy(*findField("speedRapidDuPerMinZ"), g_conf);
    zassert_equal(g_conf.z.speedRapidDuPerMin, 9600000);
    g_conf.z.accelerationDuPerSec2 = 10000;
    convertLegacy(*findField("accelerationDuPerSec2Z"), g_conf);
    zassert_equal(g_conf.z.accelerationDuPerSec2, 125000);
}

ZTEST(settings_fields, test_convert_legacy_uses_the_axis_of_the_field)
{
    g_conf.x.motorSteps = 800;
    g_conf.x.screwPitchDu = 10000;
    g_conf.x.speedStartDuPerMin = 100;
    convertLegacy(*findField("speedStartDuPerMinX"), g_conf);
    zassert_equal(g_conf.x.speedStartDuPerMin, 75000);
}

ZTEST(settings_fields, test_convert_legacy_stays_in_range)
{
    g_conf.z.motorSteps = 1;
    g_conf.z.screwPitchDu = 10000000;
    g_conf.z.speedRapidDuPerMin = 1000000;
    convertLegacy(*findField("speedRapidDuPerMinZ"), g_conf);
    zassert_equal(g_conf.z.speedRapidDuPerMin, (int32_t)findField("speedRapidDuPerMinZ")->max);
    g_conf.z.motorSteps = 1000000;
    g_conf.z.screwPitchDu = 1;
    g_conf.z.accelerationDuPerSec2 = 1;
    convertLegacy(*findField("accelerationDuPerSec2Z"), g_conf);
    zassert_equal(g_conf.z.accelerationDuPerSec2, 1);
}

ZTEST(settings_fields, test_derive_steps)
{
    deriveSteps(g_conf);
    zassert_equal(g_conf.z.speedStartSteps, 667);
    zassert_equal(g_conf.z.speedRapidSteps, 13333);
    zassert_equal(g_conf.x.speedRapidSteps, 6667);
    zassert_equal(g_conf.z.accelerationSteps, 8000);
    zassert_equal(g_conf.z.decelerationSteps, 20000);
}

ZTEST(settings_fields, test_derive_steps_follows_the_axis_values)
{
    g_conf.x.motorSteps = 3200;
    deriveSteps(g_conf);
    zassert_equal(g_conf.x.speedRapidSteps, 13333);
    zassert_equal(g_conf.z.speedRapidSteps, 13333);
    g_conf.x.speedStartDuPerMin = 1;
    g_conf.x.accelerationDuPerSec2 = 1;
    deriveSteps(g_conf);
    zassert_equal(g_conf.x.speedStartSteps, 1);
    zassert_equal(g_conf.x.accelerationSteps, 1);
}

ZTEST(settings_fields, test_json_old_name_is_converted)
{
    int64_t mask = parse("{\"speedManualMoveX\":6400,\"accelerationX\":10000}");
    zassert_equal(g_conf.x.speedRapidDuPerMin, 4800000);
    zassert_equal(g_conf.x.accelerationDuPerSec2, 125000);
    zassert_equal(mask, (int64_t)(BIT64(indexOf("speedRapidDuPerMinX")) | BIT64(indexOf("accelerationDuPerSec2X"))));
}

ZTEST(settings_fields, test_json_old_names_of_every_speed_field)
{
    parse("{\"speedStartZ\":800,\"speedRapidZ\":12800,\"accelerationZ\":10000,\"decelerationZ\":20000}");
    zassert_equal(g_conf.z.speedStartDuPerMin, 600000);
    zassert_equal(g_conf.z.speedRapidDuPerMin, 9600000);
    zassert_equal(g_conf.z.accelerationDuPerSec2, 125000);
    zassert_equal(g_conf.z.decelerationDuPerSec2, 250000);
}

ZTEST(settings_fields, test_json_old_name_uses_the_axis_values_of_the_file)
{
    parse("{\"motorStepsZ\":800,\"screwPitchDuZ\":10000,\"speedRapidZ\":1000}");
    zassert_equal(g_conf.z.speedRapidDuPerMin, 750000);
    parse("{\"speedRapidZ\":1000,\"motorStepsZ\":3200,\"screwPitchDuZ\":10000}");
    zassert_equal(g_conf.z.speedRapidDuPerMin, 187500);
}

ZTEST(settings_fields, test_json_old_name_uses_the_active_axis_values_without_them)
{
    g_conf.x.motorSteps = 800;
    g_conf.x.screwPitchDu = 10000;
    parse("{\"speedRapidX\":1000}");
    zassert_equal(g_conf.x.speedRapidDuPerMin, 750000);
}

ZTEST(settings_fields, test_json_new_name_wins)
{
    int64_t mask = parse("{\"speedRapidDuPerMinZ\":111,\"speedManualMoveZ\":222}");
    zassert_equal(g_conf.z.speedRapidDuPerMin, 111);
    zassert_equal(mask, (int64_t)BIT64(indexOf("speedRapidDuPerMinZ")));
    parse("{\"speedManualMoveZ\":333,\"speedRapidDuPerMinZ\":444}");
    zassert_equal(g_conf.z.speedRapidDuPerMin, 444);
}

ZTEST(settings_fields, test_json_newer_old_name_wins)
{
    parse("{\"speedRapidZ\":1000,\"speedManualMoveZ\":2000}");
    zassert_equal(g_conf.z.speedRapidDuPerMin, 750000);
    parse("{\"speedManualMoveX\":2000,\"speedRapidX\":1000}");
    zassert_equal(g_conf.x.speedRapidDuPerMin, 750000);
}

ZTEST(settings_fields, test_json_mask_past_bit_31)
{
    zassert_true(indexOf("handwheelLagMsX") > 31);
    int64_t mask = parse("{\"handwheelLagMsX\":250,\"encoderPpr\":600}");
    zassert_equal(mask, (int64_t)(BIT64(indexOf("handwheelLagMsX")) | BIT64(indexOf("encoderPpr"))));
}

ZTEST(settings_fields, test_json_invalid)
{
    zassert_true(parse("{\"speedRapidDuPerMinZ\":") < 0);
}

ZTEST(settings_fields, test_param_names)
{
    char name[40];
    bool length;
    zassert_true(paramName(*findField("probeFeedDuPerMin"), name, sizeof(name), length));
    zassert_str_equal(name, "_probe_feed");
    zassert_true(length);
    zassert_true(paramName(*findField("speedRapidDuPerMinX"), name, sizeof(name), length));
    zassert_str_equal(name, "_speed_rapid_x");
    zassert_true(length);
    zassert_true(paramName(*findField("accelerationDuPerSec2Z"), name, sizeof(name), length));
    zassert_str_equal(name, "_acceleration_z");
    zassert_true(length);
    zassert_true(paramName(*findField("backlashDuZ"), name, sizeof(name), length));
    zassert_str_equal(name, "_backlash_z");
    zassert_true(paramName(*findField("maxTravelMmX"), name, sizeof(name), length));
    zassert_str_equal(name, "_max_travel_mm_x");
    zassert_false(length);
    zassert_true(paramName(*findField("probeDstDu"), name, sizeof(name), length));
    zassert_str_equal(name, "_probe_dst");
    g_conf.probeOffsetDu = 1234;
    double v;
    zassert_true(paramValue("_probe_offset", g_conf, v, length));
    zassert_within(v, 1234.0, 1e-9);
    zassert_true(length);
    zassert_true(paramValue("_invert_encoder", g_conf, v, length));
    zassert_false(paramValue("_nope", g_conf, v, length));
}

ZTEST(settings_fields, test_home_fields)
{
    zassert_true(set("homeFastDuPerMinZ", "100000"));
    zassert_equal(g_conf.z.homeFastDuPerMin, 100000);
    zassert_true(set("homeSlowDuPerMinX", "20000"));
    zassert_equal(g_conf.x.homeSlowDuPerMin, 20000);
    zassert_true(set("homeLatchBackOffDuZ", "15000"));
    zassert_equal(g_conf.z.homeLatchBackOffDu, 15000);
    zassert_false(set("homeLatchBackOffDuX", "9999"));
    zassert_true(set("homePullOffDuX", "2000"));
    zassert_equal(g_conf.x.homePullOffDu, 2000);
    zassert_true(set("homePositionDuZ", "-1000"));
    zassert_equal(g_conf.z.homePositionDu, -1000);
    zassert_true(set("requireHoming", "true"));
    zassert_true(g_conf.requireHoming);
    zassert_false(set("homeFastDuPerMinZ", "-1"));
}
