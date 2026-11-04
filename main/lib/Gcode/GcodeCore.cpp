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
#include "GcodeCore.hpp"

namespace
{
    bool isSpace(char c)
    {
        return c == ' ' || c == '\t' || c == '\r' || c == '\n';
    }

    // Plain decimals only; anything after the digits starts the next word.
    bool parseNumber(const char *s, double &value, const char *&end)
    {
        const char *p = s;
        while (isSpace(*p)) {
            p++;
        }

        bool negative = false;
        if (*p == '+' || *p == '-') {
            negative = (*p == '-');
            p++;
        }

        bool anyDigits = false;
        double intPart = 0;
        while (*p >= '0' && *p <= '9') {
            intPart = intPart * 10 + (*p - '0');
            anyDigits = true;
            p++;
        }

        double fracPart = 0;
        double fracScale = 1;
        if (*p == '.') {
            p++;
            while (*p >= '0' && *p <= '9') {
                fracScale *= 10;
                fracPart = fracPart * 10 + (*p - '0');
                anyDigits = true;
                p++;
            }
        }

        if (!anyDigits) {
            return false;
        }

        value = intPart + fracPart / fracScale;
        if (negative) {
            value = -value;
        }
        end = p;
        return true;
    }
}

namespace Gcode
{
    const char *tokenize(const char *text, Words &out)
    {
        out.count = 0;

        char buf[MAX_LINE_LEN];
        size_t n = 0;
        bool inComment = false;
        for (const char *p = text; *p; p++) {
            char c = *p;
            if (c == ';') {
                break;
            }
            if (c == '(') {
                inComment = true;
                continue;
            }
            if (c == ')') {
                inComment = false;
                continue;
            }
            if (inComment) {
                continue;
            }
            if (n + 1 >= sizeof(buf)) {
                return "line too long";
            }
            buf[n++] = c;
        }
        buf[n] = '\0';

        size_t i = 0;
        while (isSpace(buf[i])) {
            i++;
        }
        if (buf[i] == '\0') {
            return nullptr;
        }
        if (buf[i] == '%') {
            size_t k = i + 1;
            while (isSpace(buf[k])) {
                k++;
            }
            if (buf[k] == '\0') {
                return nullptr;
            }
        }

        while (buf[i] != '\0') {
            while (isSpace(buf[i])) {
                i++;
            }
            if (buf[i] == '\0') {
                break;
            }

            char letter = buf[i];
            if (letter >= 'a' && letter <= 'z') {
                letter = (char)(letter - 'a' + 'A');
            }
            if (letter < 'A' || letter > 'Z') {
                return "malformed word";
            }
            i++;

            double value;
            const char *numEnd;
            if (!parseNumber(&buf[i], value, numEnd)) {
                return "malformed word";
            }
            i += (size_t)(numEnd - &buf[i]);

            if (letter == 'N') {
                continue;
            }
            if (out.count >= MAX_WORDS) {
                return "too many words";
            }
            out.words[out.count].letter = letter;
            out.words[out.count].value = value;
            out.count++;
        }
        return nullptr;
    }

    const char *interpretLine(Flavor &flavor, const char *text, uint32_t lineNo, StepSink &sink)
    {
        Words words;
        const char *err = tokenize(text, words);
        if (err) {
            return err;
        }
        if (words.count == 0) {
            return nullptr;
        }
        return flavor.line(words, lineNo, sink);
    }
}
