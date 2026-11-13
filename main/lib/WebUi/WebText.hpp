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
#ifndef WebText_h
#define WebText_h

#include <stddef.h>

namespace WebHttp
{
    // Value of name in the URL's query string, or nullptr; ends at '&' or NUL.
    const char *urlQueryValue(const char *url, const char *name);
    // The raw value into out; false if absent or too long.
    bool urlQuery(const char *url, const char *name, char *out, size_t outMax);

    // Appends JSON into a fixed buffer; ok turns false once it doesn't fit.
    struct JsonWriter
    {
        char *buf;
        size_t max;
        size_t pos = 0;
        bool ok = true;

        JsonWriter(char *b, size_t m) : buf(b), max(m)
        {}

        JsonWriter &raw(const char *s);
        JsonWriter &str(const char *s); // quoted and escaped
        JsonWriter &strOrNull(const char *s); // null when empty
        __attribute__((format(printf, 2, 3))) JsonWriter &fmt(const char *format, ...);
    };
}

#endif
