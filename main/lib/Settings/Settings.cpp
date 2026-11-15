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
#include "Events.hpp"

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

    bool loadKey(const char *key, const Field &f)
    {
        void *ptr = Settings::fieldPtr(f, g_active);
        switch (f.type) {
        case FieldType::I32:
        {
            int32_t v;
            if (PersistBlob::load(NVS_NS, key, &v, sizeof(v)) == sizeof(v)) {
                *(int32_t *)ptr = v;
                return true;
            }
            break;
        }
        case FieldType::Bool:
        {
            uint8_t v;
            if (PersistBlob::load(NVS_NS, key, &v, sizeof(v)) == sizeof(v)) {
                *(bool *)ptr = v != 0;
                return true;
            }
            break;
        }
        case FieldType::Float:
        {
            float v;
            if (PersistBlob::load(NVS_NS, key, &v, sizeof(v)) == sizeof(v)) {
                *(float *)ptr = v;
                return true;
            }
            break;
        }
        }
        return false;
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

    void loadField(const Field &f)
    {
        if (loadKey(f.name, f)) {
            return;
        }
        const char *old;
        for (size_t i = 0; (old = Settings::oldName(f, i)); i++) {
            if (loadKey(old, f)) {
                Settings::convertLegacy(f, g_active);
                saveField(f);
                return;
            }
        }
    }

    void applyChanged()
    {
        Settings::deriveSteps(g_active);
        Events::SettingsChanged msg = {};
        Events::publish(&SETTINGS_CHAN, &msg);
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
        deriveSteps(g_active);
    }

    bool save()
    {
        bool ok = true;
        for (const Field &f : FIELDS) {
            ok = saveField(f) && ok;
        }
        applyChanged();
        return ok;
    }

    bool setByName(const char *name, const char *value, char *out, size_t outMax)
    {
        const Field *f = findField(name);
        if (!f) {
            snprintf(out, outMax, "unknown field '%s'", name);
            return false;
        }
        if (!parseField(*f, value, g_active, out, outMax)) {
            return false;
        }
        bool ok = saveField(*f);
        applyChanged();
        return ok;
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
        int64_t mask = parseJson(buf, len, temp);
        if (mask < 0) {
            snprintf(out, outMax, "invalid json");
            return false;
        }
        if (!validate(temp, out, outMax)) {
            return false;
        }
        for (size_t i = 0; i < CONF_JSON_FIELDS; i++) {
            if (mask & BIT64(i)) {
                const Field &f = FIELDS[i];
                memcpy(fieldPtr(f, g_active), fieldPtr(f, temp), fieldSize(f));
                saveField(f);
            }
        }
        applyChanged();
        return true;
    }
}
