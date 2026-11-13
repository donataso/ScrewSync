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
#include "SettingsFields.hpp"
#include "PersistBlob.hpp"

#include <stdio.h>
#include <string.h>
#include <zephyr/data/json.h>
#include <zephyr/sys/util.h>

namespace
{
    constexpr const char *NVS_NS = "settings";

    using Settings::Field;
    using Settings::FieldType;
    using Settings::g_active;

    void loadField(const Field &f)
    {
        void *ptr = Settings::fieldPtr(f, g_active);
        switch (f.type) {
        case FieldType::I32:
        {
            int32_t v;
            if (PersistBlob::load(NVS_NS, f.name, &v, sizeof(v)) == sizeof(v)) {
                *(int32_t *)ptr = v;
            }
            break;
        }
        case FieldType::Bool:
        {
            uint8_t v;
            if (PersistBlob::load(NVS_NS, f.name, &v, sizeof(v)) == sizeof(v)) {
                *(bool *)ptr = v != 0;
            }
            break;
        }
        case FieldType::Float:
        {
            float v;
            if (PersistBlob::load(NVS_NS, f.name, &v, sizeof(v)) == sizeof(v)) {
                *(float *)ptr = v;
            }
            break;
        }
        }
    }

    bool saveField(const Field &f)
    {
        void *ptr = Settings::fieldPtr(f, g_active);
        switch (f.type) {
        case FieldType::I32:
            return PersistBlob::save(NVS_NS, f.name, ptr, sizeof(int32_t));
        case FieldType::Bool:
        {
            uint8_t v = *(bool *)ptr ? 1 : 0;
            return PersistBlob::save(NVS_NS, f.name, &v, sizeof(v));
        }
        case FieldType::Float:
            return PersistBlob::save(NVS_NS, f.name, ptr, sizeof(float));
        }
        return false;
    }
}

namespace Settings
{
    Conf g_active;

    void load()
    {
        for (const Field &f : FIELDS) {
            loadField(f);
        }
    }

    bool save()
    {
        bool ok = true;
        for (const Field &f : FIELDS) {
            ok = saveField(f) && ok;
        }
        return ok;
    }

    bool setByName(const char *name, const char *value, char *out, size_t outMax)
    {
        const Field *f = findField(name);
        if (!f) {
            snprintf(out, outMax, "unknown field '%s'", name);
            return false;
        }
        return parseField(*f, value, g_active, out, outMax) && saveField(*f);
    }

    void formatAll(char *out, size_t outMax)
    {
        formatAll(g_active, out, outMax);
    }

    bool replace(const Conf &conf)
    {
        g_active = conf;
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

        Conf temp = g_active;
        int64_t mask = json_obj_parse(buf, len, CONF_JSON_DESCR, ARRAY_SIZE(CONF_JSON_DESCR), &temp);
        if (mask < 0) {
            snprintf(out, outMax, "invalid json");
            return false;
        }
        if (!validate(temp, out, outMax)) {
            return false;
        }
        for (size_t i = 0; i < CONF_JSON_FIELDS; i++) {
            if (mask & BIT(i)) {
                const Field &f = FIELDS[i];
                memcpy(fieldPtr(f, g_active), fieldPtr(f, temp), fieldSize(f));
                saveField(f);
            }
        }
        return true;
    }
}
