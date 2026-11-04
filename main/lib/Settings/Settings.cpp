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
#include "Settings.hpp"
#include "PersistBlob.hpp"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <zephyr/data/json.h>
#include <zephyr/sys/util.h>

namespace
{
    constexpr const char *NVS_NS = "settings";

    enum class Type
    {
        I32,
        Bool,
        Float,
    };

    struct FieldDef
    {
        const char *name;
        Type type;
        void *ptr;
        float min, max; // unused for Bool
    };

    using Settings::active;

    const FieldDef FIELDS[] = {
        {"encoderPpr", Type::I32, &active.encoderPpr, 1, 15000},
        {"encoderBacklash", Type::I32, &active.encoderBacklash, 0, 30000},
        {"invertEncoder", Type::Bool, &active.invertEncoder, 0, 0},
        {"axisEncoderBacklash", Type::I32, &active.axisEncoderBacklash, 0, 30000},
        {"stepTimeMs", Type::I32, &active.stepTimeMs, 1, 10000},
        {"delayBetweenStepsMs", Type::I32, &active.delayBetweenStepsMs, 0, 10000},
        {"safeDistanceDu", Type::I32, &active.safeDistanceDu, 0, 10000000},
        {"slotLeftReductionDu", Type::I32, &active.slotLeftReductionDu, 0, 10000000},
        {"statusPushIntervalMs", Type::I32, &active.statusPushIntervalMs, 50, 10000},

        {"probePolarityNc", Type::Bool, &active.probePolarityNc, 0, 0},
        {"probeFeedDuPerMin", Type::I32, &active.probeFeedDuPerMin, 1, 60000000},
        {"reprobeFeedDuPerMin", Type::I32, &active.reprobeFeedDuPerMin, 1, 60000000},
        {"probeBackOffDu", Type::I32, &active.probeBackOffDu, 1, 10000000},

        {"screwPitchDuZ", Type::I32, &active.z.screwPitchDu, 1, 10000000},
        {"motorStepsZ", Type::I32, &active.z.motorSteps, 1, 1000000},
        {"speedStartZ", Type::I32, &active.z.speedStart, 1, 1000000},
        {"accelerationZ", Type::I32, &active.z.acceleration, 1, 100000000},
        {"speedManualMoveZ", Type::I32, &active.z.speedManualMove, 1, 1000000},
        {"invertDirectionZ", Type::Bool, &active.z.invertDirection, 0, 0},
        {"invertEnableZ", Type::Bool, &active.z.invertEnable, 0, 0},
        {"maxTravelMmZ", Type::I32, &active.z.maxTravelMm, 1, 10000},
        {"backlashDuZ", Type::I32, &active.z.backlashDu, 0, 10000000},
        {"handwheelPprZ", Type::Float, &active.z.handwheelPpr, 1, 100000},
        {"invertHandwheelZ", Type::Bool, &active.z.invertHandwheel, 0, 0},

        {"screwPitchDuX", Type::I32, &active.x.screwPitchDu, 1, 10000000},
        {"motorStepsX", Type::I32, &active.x.motorSteps, 1, 1000000},
        {"speedStartX", Type::I32, &active.x.speedStart, 1, 1000000},
        {"accelerationX", Type::I32, &active.x.acceleration, 1, 100000000},
        {"speedManualMoveX", Type::I32, &active.x.speedManualMove, 1, 1000000},
        {"invertDirectionX", Type::Bool, &active.x.invertDirection, 0, 0},
        {"invertEnableX", Type::Bool, &active.x.invertEnable, 0, 0},
        {"maxTravelMmX", Type::I32, &active.x.maxTravelMm, 1, 10000},
        {"backlashDuX", Type::I32, &active.x.backlashDu, 0, 10000000},
        {"handwheelPprX", Type::Float, &active.x.handwheelPpr, 1, 100000},
        {"invertHandwheelX", Type::Bool, &active.x.invertHandwheel, 0, 0},
    };
    constexpr size_t FIELD_COUNT = sizeof(FIELDS) / sizeof(FIELDS[0]);

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

    void loadField(const FieldDef &f)
    {
        switch (f.type)
        {
        case Type::I32:
        {
            int32_t v;
            if (PersistBlob::load(NVS_NS, f.name, &v, sizeof(v)) == sizeof(v)) {
                *(int32_t *)f.ptr = v;
            }
            break;
        }
        case Type::Bool:
        {
            uint8_t v;
            if (PersistBlob::load(NVS_NS, f.name, &v, sizeof(v)) == sizeof(v)) {
                *(bool *)f.ptr = v != 0;
            }
            break;
        }
        case Type::Float:
        {
            float v;
            if (PersistBlob::load(NVS_NS, f.name, &v, sizeof(v)) == sizeof(v)) {
                *(float *)f.ptr = v;
            }
            break;
        }
        }
    }

    bool saveField(const FieldDef &f)
    {
        switch (f.type)
        {
        case Type::I32:
            return PersistBlob::save(NVS_NS, f.name, f.ptr, sizeof(int32_t));
        case Type::Bool:
        {
            uint8_t v = *(bool *)f.ptr ? 1 : 0;
            return PersistBlob::save(NVS_NS, f.name, &v, sizeof(v));
        }
        case Type::Float:
            return PersistBlob::save(NVS_NS, f.name, f.ptr, sizeof(float));
        }
        return false;
    }

    const void *fieldIn(const FieldDef &f, const Settings::Conf &conf)
    {
        return (const uint8_t *)&conf + ((const uint8_t *)f.ptr - (const uint8_t *)&active);
    }

    size_t fieldSize(const FieldDef &f)
    {
        return f.type == Type::Bool ? sizeof(bool) : sizeof(int32_t);
    }

    void formatField(const FieldDef &f, char *out, size_t outMax)
    {
        switch (f.type)
        {
        case Type::I32:
            snprintf(out, outMax, "%ld", (long)*(int32_t *)f.ptr);
            break;
        case Type::Bool:
            snprintf(out, outMax, "%s", *(bool *)f.ptr ? "true" : "false");
            break;
        case Type::Float:
            snprintf(out, outMax, "%g", (double)*(float *)f.ptr);
            break;
        }
    }
}

namespace Settings
{
    Conf active;

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
        JSON_OBJ_DESCR_PRIM_NAMED(Settings::Conf, "speedManualMoveZ", z.speedManualMove, JSON_TOK_NUMBER),
        JSON_OBJ_DESCR_PRIM_NAMED(Settings::Conf, "invertDirectionZ", z.invertDirection, JSON_TOK_TRUE),
        JSON_OBJ_DESCR_PRIM_NAMED(Settings::Conf, "invertEnableZ", z.invertEnable, JSON_TOK_TRUE),
        JSON_OBJ_DESCR_PRIM_NAMED(Settings::Conf, "maxTravelMmZ", z.maxTravelMm, JSON_TOK_NUMBER),
        JSON_OBJ_DESCR_PRIM_NAMED(Settings::Conf, "backlashDuZ", z.backlashDu, JSON_TOK_NUMBER),
        JSON_OBJ_DESCR_PRIM_NAMED(Settings::Conf, "handwheelPprZ", z.handwheelPpr, JSON_TOK_FLOAT_FP),
        JSON_OBJ_DESCR_PRIM_NAMED(Settings::Conf, "invertHandwheelZ", z.invertHandwheel, JSON_TOK_TRUE),

        JSON_OBJ_DESCR_PRIM_NAMED(Settings::Conf, "screwPitchDuX", x.screwPitchDu, JSON_TOK_NUMBER),
        JSON_OBJ_DESCR_PRIM_NAMED(Settings::Conf, "motorStepsX", x.motorSteps, JSON_TOK_NUMBER),
        JSON_OBJ_DESCR_PRIM_NAMED(Settings::Conf, "speedStartX", x.speedStart, JSON_TOK_NUMBER),
        JSON_OBJ_DESCR_PRIM_NAMED(Settings::Conf, "accelerationX", x.acceleration, JSON_TOK_NUMBER),
        JSON_OBJ_DESCR_PRIM_NAMED(Settings::Conf, "speedManualMoveX", x.speedManualMove, JSON_TOK_NUMBER),
        JSON_OBJ_DESCR_PRIM_NAMED(Settings::Conf, "invertDirectionX", x.invertDirection, JSON_TOK_TRUE),
        JSON_OBJ_DESCR_PRIM_NAMED(Settings::Conf, "invertEnableX", x.invertEnable, JSON_TOK_TRUE),
        JSON_OBJ_DESCR_PRIM_NAMED(Settings::Conf, "maxTravelMmX", x.maxTravelMm, JSON_TOK_NUMBER),
        JSON_OBJ_DESCR_PRIM_NAMED(Settings::Conf, "backlashDuX", x.backlashDu, JSON_TOK_NUMBER),
        JSON_OBJ_DESCR_PRIM_NAMED(Settings::Conf, "handwheelPprX", x.handwheelPpr, JSON_TOK_FLOAT_FP),
        JSON_OBJ_DESCR_PRIM_NAMED(Settings::Conf, "invertHandwheelX", x.invertHandwheel, JSON_TOK_TRUE),
    };

    void load()
    {
        for (const FieldDef &f : FIELDS) {
            loadField(f);
        }
    }

    bool save()
    {
        bool ok = true;
        for (const FieldDef &f : FIELDS) {
            ok = saveField(f) && ok;
        }
        return ok;
    }

    bool setByName(const char *name, const char *value, char *out, size_t outMax)
    {
        for (const FieldDef &f : FIELDS)
        {
            if (strcmp(f.name, name) != 0) {
                continue;
            }

            switch (f.type)
            {
            case Type::I32:
            {
                char *end;
                long v = strtol(value, &end, 10);
                if (end == value || *end || v < f.min || v > f.max)
                {
                    snprintf(out, outMax, "range [%ld, %ld]", (long)f.min, (long)f.max);
                    return false;
                }
                *(int32_t *)f.ptr = (int32_t)v;
                break;
            }
            case Type::Bool:
            {
                bool v;
                if (!parseBool(value, v))
                {
                    snprintf(out, outMax, "expected 0|1|true|false|on|off");
                    return false;
                }
                *(bool *)f.ptr = v;
                break;
            }
            case Type::Float:
            {
                char *end;
                float v = strtof(value, &end);
                if (end == value || *end || v < f.min || v > f.max)
                {
                    snprintf(out, outMax, "range [%g, %g]", (double)f.min, (double)f.max);
                    return false;
                }
                *(float *)f.ptr = v;
                break;
            }
            }
            return saveField(f);
        }
        snprintf(out, outMax, "unknown field '%s'", name);
        return false;
    }

    void formatAll(char *out, size_t outMax)
    {
        int n = snprintf(out, outMax, "{");
        char val[32];
        bool first = true;
        for (const FieldDef &f : FIELDS)
        {
            if (n < 0 || (size_t)n >= outMax) {
                break;
            }
            formatField(f, val, sizeof(val));
            n += snprintf(out + n, outMax - (size_t)n, "%s\"%s\":%s", first ? "" : ",", f.name, val);
            first = false;
        }
        if (n >= 0 && (size_t)n < outMax) {
            snprintf(out + n, outMax - (size_t)n, "}");
        }
    }

    bool validate(const Conf &conf, char *out, size_t outMax)
    {
        for (const FieldDef &f : FIELDS)
        {
            const void *src = fieldIn(f, conf);
            if (f.type == Type::I32)
            {
                int32_t v = *(const int32_t *)src;
                if (v < f.min || v > f.max) {
                    snprintf(out, outMax, "%s: range [%ld, %ld]", f.name, (long)f.min, (long)f.max);
                    return false;
                }
            }
            else if (f.type == Type::Float)
            {
                float v = *(const float *)src;
                if (!(v >= f.min && v <= f.max)) {
                    snprintf(out, outMax, "%s: range [%g, %g]", f.name, (double)f.min, (double)f.max);
                    return false;
                }
            }
        }
        return true;
    }

    bool replace(const Conf &conf)
    {
        active = conf;
        return save();
    }

    bool setAllFromJson(const char *json, size_t len, char *out, size_t outMax)
    {
        constexpr size_t BUF = 256;
        if (len >= BUF) {
            snprintf(out, outMax, "json too long");
            return false;
        }
        char buf[BUF];
        memcpy(buf, json, len);
        buf[len] = '\0';

        Conf temp = active;
        int64_t mask = json_obj_parse(buf, len, CONF_JSON_DESCR, ARRAY_SIZE(CONF_JSON_DESCR), &temp);
        if (mask < 0) {
            snprintf(out, outMax, "invalid json");
            return false;
        }
        if (!validate(temp, out, outMax)) {
            return false;
        }

        for (size_t i = 0; i < FIELD_COUNT; i++)
        {
            if (mask & BIT(i)) {
                const FieldDef &f = FIELDS[i];
                memcpy(f.ptr, fieldIn(f, temp), fieldSize(f));
                saveField(f);
            }
        }
        return true;
    }
}
