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
#include "JsonSpan.hpp"
#include <ctype.h>
#include <string.h>

namespace
{
    // p at the opening quote; past the closing quote, or nullptr.
    char *skipString(char *p, const char *end)
    {
        for (p++; p < end; p++) {
            if (*p == '\\') {
                p++;
            } else if (*p == '"') {
                return p + 1;
            }
        }
        return nullptr;
    }

    char *skipSpace(char *p, const char *end)
    {
        while (p < end && isspace((unsigned char)*p)) {
            p++;
        }
        return p;
    }
}

namespace JsonSpan
{
    bool findObject(char *json, size_t len, const char *key, char *&objStart, size_t &objLen)
    {
        const char *end = json + len;
        size_t keyLen = strlen(key);
        char *found = nullptr;
        int depth = 0;
        char *p = json;
        while (p < end) {
            if (*p == '"') {
                char *str = p;
                p = skipString(p, end);
                if (!p) {
                    return false;
                }
                bool isKey = depth == 1 && !found && (size_t)(p - str - 2) == keyLen && !memcmp(str + 1, key, keyLen);
                char *value = isKey ? skipSpace(p, end) : nullptr;
                if (value && value < end && *value == ':') {
                    value = skipSpace(value + 1, end);
                    if (value < end && *value == '{') {
                        found = value;
                        p = value;
                    }
                }
                continue;
            }
            if (*p == '{' || *p == '[') {
                depth++;
            } else if (*p == '}' || *p == ']') {
                depth--;
                if (found && depth == 1) {
                    objStart = found;
                    objLen = (size_t)(p + 1 - found);
                    return true;
                }
            }
            p++;
        }
        return false;
    }
}
