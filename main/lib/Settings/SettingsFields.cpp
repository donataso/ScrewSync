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
#include "SettingsFields.hpp"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

namespace
{
    using Settings::Field;
    using Settings::FieldType;

    bool parseBool(const char *s, bool &out)
    {
        if (!strcmp(s, "1") || !strcmp(s, "true") || !strcmp(s, "on")) {
            out = true;
            return true;
        }
        if (!strcmp(s, "0") || !strcmp(s, "false") || !strcmp(s, "off")) {
            out = false;
            return true;
        }
        return false;
    }

    void formatField(const Field &f, const Settings::Conf &conf, char *out, size_t outMax)
    {
        const void *ptr = Settings::fieldPtr(f, conf);
        switch (f.type) {
        case FieldType::I32:
            snprintf(out, outMax, "%ld", (long)*(const int32_t *)ptr);
            break;
        case FieldType::Bool:
            snprintf(out, outMax, "%s", *(const bool *)ptr ? "true" : "false");
            break;
        case FieldType::Float:
            snprintf(out, outMax, "%g", (double)*(const float *)ptr);
            break;
        }
    }
}

namespace Settings
{
    const Field FIELDS[CONF_JSON_FIELDS] = {
        {"encoderPpr", FieldType::I32, offsetof(Conf, encoderPpr), 1, 15000},
        {"encoderBacklash", FieldType::I32, offsetof(Conf, encoderBacklash), 0, 30000},
        {"invertEncoder", FieldType::Bool, offsetof(Conf, invertEncoder), 0, 0},
        {"axisEncoderBacklash", FieldType::I32, offsetof(Conf, axisEncoderBacklash), 0, 30000},
        {"stepTimeMs", FieldType::I32, offsetof(Conf, stepTimeMs), 1, 10000},
        {"delayBetweenStepsMs", FieldType::I32, offsetof(Conf, delayBetweenStepsMs), 0, 10000},
        {"safeDistanceDu", FieldType::I32, offsetof(Conf, safeDistanceDu), 0, 10000000},
        {"slotLeftReductionDu", FieldType::I32, offsetof(Conf, slotLeftReductionDu), 0, 10000000},
        {"statusPushIntervalMs", FieldType::I32, offsetof(Conf, statusPushIntervalMs), 50, 10000},
        {"probePolarityNc", FieldType::Bool, offsetof(Conf, probePolarityNc), 0, 0},
        {"probeFeedDuPerMin", FieldType::I32, offsetof(Conf, probeFeedDuPerMin), 1, 60000000},
        {"reprobeFeedDuPerMin", FieldType::I32, offsetof(Conf, reprobeFeedDuPerMin), 1, 60000000},
        {"probeBackOffDu", FieldType::I32, offsetof(Conf, probeBackOffDu), 1, 10000000},
        {"screwPitchDuZ", FieldType::I32, offsetof(Conf, z.screwPitchDu), 1, 10000000},
        {"motorStepsZ", FieldType::I32, offsetof(Conf, z.motorSteps), 1, 1000000},
        {"speedStartZ", FieldType::I32, offsetof(Conf, z.speedStart), 1, 1000000},
        {"accelerationZ", FieldType::I32, offsetof(Conf, z.acceleration), 1, 100000000},
        {"decelerationZ", FieldType::I32, offsetof(Conf, z.deceleration), 1, 100000000},
        {"speedManualMoveZ", FieldType::I32, offsetof(Conf, z.speedManualMove), 1, 1000000},
        {"invertDirectionZ", FieldType::Bool, offsetof(Conf, z.invertDirection), 0, 0},
        {"invertEnableZ", FieldType::Bool, offsetof(Conf, z.invertEnable), 0, 0},
        {"maxTravelMmZ", FieldType::I32, offsetof(Conf, z.maxTravelMm), 1, 10000},
        {"backlashDuZ", FieldType::I32, offsetof(Conf, z.backlashDu), 0, 10000000},
        {"handwheelPprZ", FieldType::Float, offsetof(Conf, z.handwheelPpr), 1, 100000},
        {"invertHandwheelZ", FieldType::Bool, offsetof(Conf, z.invertHandwheel), 0, 0},
        {"handwheelLagMsZ", FieldType::I32, offsetof(Conf, z.handwheelLagMs), 1, 1000},
        {"screwPitchDuX", FieldType::I32, offsetof(Conf, x.screwPitchDu), 1, 10000000},
        {"motorStepsX", FieldType::I32, offsetof(Conf, x.motorSteps), 1, 1000000},
        {"speedStartX", FieldType::I32, offsetof(Conf, x.speedStart), 1, 1000000},
        {"accelerationX", FieldType::I32, offsetof(Conf, x.acceleration), 1, 100000000},
        {"decelerationX", FieldType::I32, offsetof(Conf, x.deceleration), 1, 100000000},
        {"speedManualMoveX", FieldType::I32, offsetof(Conf, x.speedManualMove), 1, 1000000},
        {"invertDirectionX", FieldType::Bool, offsetof(Conf, x.invertDirection), 0, 0},
        {"invertEnableX", FieldType::Bool, offsetof(Conf, x.invertEnable), 0, 0},
        {"maxTravelMmX", FieldType::I32, offsetof(Conf, x.maxTravelMm), 1, 10000},
        {"backlashDuX", FieldType::I32, offsetof(Conf, x.backlashDu), 0, 10000000},
        {"handwheelPprX", FieldType::Float, offsetof(Conf, x.handwheelPpr), 1, 100000},
        {"invertHandwheelX", FieldType::Bool, offsetof(Conf, x.invertHandwheel), 0, 0},
        {"handwheelLagMsX", FieldType::I32, offsetof(Conf, x.handwheelLagMs), 1, 1000},
    };

    const struct json_obj_descr CONF_JSON_DESCR[] = {
        JSON_OBJ_DESCR_PRIM(Settings::Conf, encoderPpr, JSON_TOK_NUMBER),
        JSON_OBJ_DESCR_PRIM(Settings::Conf, encoderBacklash, JSON_TOK_NUMBER),
        JSON_OBJ_DESCR_PRIM(Settings::Conf, invertEncoder, JSON_TOK_TRUE),
        JSON_OBJ_DESCR_PRIM(Settings::Conf, axisEncoderBacklash, JSON_TOK_NUMBER),
        JSON_OBJ_DESCR_PRIM(Settings::Conf, stepTimeMs, JSON_TOK_NUMBER),
        JSON_OBJ_DESCR_PRIM(Settings::Conf, delayBetweenStepsMs, JSON_TOK_NUMBER),
        JSON_OBJ_DESCR_PRIM(Settings::Conf, safeDistanceDu, JSON_TOK_NUMBER),
        JSON_OBJ_DESCR_PRIM(Settings::Conf, slotLeftReductionDu, JSON_TOK_NUMBER),
        JSON_OBJ_DESCR_PRIM(Settings::Conf, statusPushIntervalMs, JSON_TOK_NUMBER),

        JSON_OBJ_DESCR_PRIM(Settings::Conf, probePolarityNc, JSON_TOK_TRUE),
        JSON_OBJ_DESCR_PRIM(Settings::Conf, probeFeedDuPerMin, JSON_TOK_NUMBER),
        JSON_OBJ_DESCR_PRIM(Settings::Conf, reprobeFeedDuPerMin, JSON_TOK_NUMBER),
        JSON_OBJ_DESCR_PRIM(Settings::Conf, probeBackOffDu, JSON_TOK_NUMBER),

        JSON_OBJ_DESCR_PRIM_NAMED(Settings::Conf, "screwPitchDuZ", z.screwPitchDu, JSON_TOK_NUMBER),
        JSON_OBJ_DESCR_PRIM_NAMED(Settings::Conf, "motorStepsZ", z.motorSteps, JSON_TOK_NUMBER),
        JSON_OBJ_DESCR_PRIM_NAMED(Settings::Conf, "speedStartZ", z.speedStart, JSON_TOK_NUMBER),
        JSON_OBJ_DESCR_PRIM_NAMED(Settings::Conf, "accelerationZ", z.acceleration, JSON_TOK_NUMBER),
        JSON_OBJ_DESCR_PRIM_NAMED(Settings::Conf, "decelerationZ", z.deceleration, JSON_TOK_NUMBER),
        JSON_OBJ_DESCR_PRIM_NAMED(Settings::Conf, "speedManualMoveZ", z.speedManualMove, JSON_TOK_NUMBER),
        JSON_OBJ_DESCR_PRIM_NAMED(Settings::Conf, "invertDirectionZ", z.invertDirection, JSON_TOK_TRUE),
        JSON_OBJ_DESCR_PRIM_NAMED(Settings::Conf, "invertEnableZ", z.invertEnable, JSON_TOK_TRUE),
        JSON_OBJ_DESCR_PRIM_NAMED(Settings::Conf, "maxTravelMmZ", z.maxTravelMm, JSON_TOK_NUMBER),
        JSON_OBJ_DESCR_PRIM_NAMED(Settings::Conf, "backlashDuZ", z.backlashDu, JSON_TOK_NUMBER),
        JSON_OBJ_DESCR_PRIM_NAMED(Settings::Conf, "handwheelPprZ", z.handwheelPpr, JSON_TOK_FLOAT_FP),
        JSON_OBJ_DESCR_PRIM_NAMED(Settings::Conf, "invertHandwheelZ", z.invertHandwheel, JSON_TOK_TRUE),
        JSON_OBJ_DESCR_PRIM_NAMED(Settings::Conf, "handwheelLagMsZ", z.handwheelLagMs, JSON_TOK_NUMBER),

        JSON_OBJ_DESCR_PRIM_NAMED(Settings::Conf, "screwPitchDuX", x.screwPitchDu, JSON_TOK_NUMBER),
        JSON_OBJ_DESCR_PRIM_NAMED(Settings::Conf, "motorStepsX", x.motorSteps, JSON_TOK_NUMBER),
        JSON_OBJ_DESCR_PRIM_NAMED(Settings::Conf, "speedStartX", x.speedStart, JSON_TOK_NUMBER),
        JSON_OBJ_DESCR_PRIM_NAMED(Settings::Conf, "accelerationX", x.acceleration, JSON_TOK_NUMBER),
        JSON_OBJ_DESCR_PRIM_NAMED(Settings::Conf, "decelerationX", x.deceleration, JSON_TOK_NUMBER),
        JSON_OBJ_DESCR_PRIM_NAMED(Settings::Conf, "speedManualMoveX", x.speedManualMove, JSON_TOK_NUMBER),
        JSON_OBJ_DESCR_PRIM_NAMED(Settings::Conf, "invertDirectionX", x.invertDirection, JSON_TOK_TRUE),
        JSON_OBJ_DESCR_PRIM_NAMED(Settings::Conf, "invertEnableX", x.invertEnable, JSON_TOK_TRUE),
        JSON_OBJ_DESCR_PRIM_NAMED(Settings::Conf, "maxTravelMmX", x.maxTravelMm, JSON_TOK_NUMBER),
        JSON_OBJ_DESCR_PRIM_NAMED(Settings::Conf, "backlashDuX", x.backlashDu, JSON_TOK_NUMBER),
        JSON_OBJ_DESCR_PRIM_NAMED(Settings::Conf, "handwheelPprX", x.handwheelPpr, JSON_TOK_FLOAT_FP),
        JSON_OBJ_DESCR_PRIM_NAMED(Settings::Conf, "invertHandwheelX", x.invertHandwheel, JSON_TOK_TRUE),
        JSON_OBJ_DESCR_PRIM_NAMED(Settings::Conf, "handwheelLagMsX", x.handwheelLagMs, JSON_TOK_NUMBER),
    };

    const Field *findField(const char *name)
    {
        for (const Field &f : FIELDS) {
            if (!strcmp(f.name, name)) {
                return &f;
            }
        }
        return nullptr;
    }

    void *fieldPtr(const Field &f, Conf &conf)
    {
        return (uint8_t *)&conf + f.offset;
    }

    const void *fieldPtr(const Field &f, const Conf &conf)
    {
        return (const uint8_t *)&conf + f.offset;
    }

    size_t fieldSize(const Field &f)
    {
        return f.type == FieldType::Bool ? sizeof(bool) : sizeof(int32_t);
    }

    bool parseField(const Field &f, const char *value, Conf &conf, char *out, size_t outMax)
    {
        void *ptr = fieldPtr(f, conf);
        switch (f.type) {
        case FieldType::I32:
        {
            char *end;
            long v = strtol(value, &end, 10);
            if (end == value || *end || (double)v < f.min || (double)v > f.max) {
                snprintf(out, outMax, "range [%ld, %ld]", (long)f.min, (long)f.max);
                return false;
            }
            *(int32_t *)ptr = (int32_t)v;
            break;
        }
        case FieldType::Bool:
        {
            bool v;
            if (!parseBool(value, v)) {
                snprintf(out, outMax, "expected 0|1|true|false|on|off");
                return false;
            }
            *(bool *)ptr = v;
            break;
        }
        case FieldType::Float:
        {
            char *end;
            float v = strtof(value, &end);
            if (end == value || *end || (double)v < f.min || (double)v > f.max) {
                snprintf(out, outMax, "range [%g, %g]", (double)f.min, (double)f.max);
                return false;
            }
            *(float *)ptr = v;
            break;
        }
        }
        return true;
    }

    void formatAll(const Conf &conf, char *out, size_t outMax)
    {
        int n = snprintf(out, outMax, "{");
        char val[32];
        bool first = true;
        for (const Field &f : FIELDS) {
            if (n < 0 || (size_t)n >= outMax) {
                break;
            }
            formatField(f, conf, val, sizeof(val));
            n += snprintf(out + n, outMax - (size_t)n, "%s\"%s\":%s", first ? "" : ",", f.name, val);
            first = false;
        }
        if (n >= 0 && (size_t)n < outMax) {
            snprintf(out + n, outMax - (size_t)n, "}");
        }
    }

    bool validate(const Conf &conf, char *out, size_t outMax)
    {
        for (const Field &f : FIELDS) {
            const void *src = fieldPtr(f, conf);
            if (f.type == FieldType::I32) {
                int32_t v = *(const int32_t *)src;
                if ((double)v < f.min || (double)v > f.max) {
                    snprintf(out, outMax, "%s: range [%ld, %ld]", f.name, (long)f.min, (long)f.max);
                    return false;
                }
            } else if (f.type == FieldType::Float) {
                float v = *(const float *)src;
                if (!((double)v >= f.min && (double)v <= f.max)) {
                    snprintf(out, outMax, "%s: range [%g, %g]", f.name, (double)f.min, (double)f.max);
                    return false;
                }
            }
        }
        return true;
    }
}
