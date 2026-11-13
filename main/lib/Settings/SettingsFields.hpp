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
#ifndef SettingsFields_h
#define SettingsFields_h

#include "Settings.hpp"

// The fields of a Conf by name, with their ranges. In the order of
// CONF_JSON_DESCR.
namespace Settings
{
    enum class FieldType : uint8_t
    {
        I32,
        Bool,
        Float,
    };

    struct Field
    {
        const char *name;
        FieldType type;
        size_t offset;
        double min, max; // unused for Bool
    };

    extern const Field FIELDS[CONF_JSON_FIELDS];

    const Field *findField(const char *name);
    void *fieldPtr(const Field &field, Conf &conf);
    const void *fieldPtr(const Field &field, const Conf &conf);
    size_t fieldSize(const Field &field);
    // Parses value into the field of conf; false with the reason in out.
    bool parseField(const Field &field, const char *value, Conf &conf, char *out, size_t outMax);
    // A JSON object of every field.
    void formatAll(const Conf &conf, char *out, size_t outMax);
}

#endif
