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
#include "GcodeDialects.hpp"
#include "GcodeInterpreter.hpp"
#include "GcodeLathecode.hpp"
#include <string.h>

namespace
{
    using namespace Gcode;

    // Standard words, no translation.
    class Linuxcnc : public Dialect
    {
    public:
        explicit Linuxcnc(const char *id) : id_(id)
        {}
        const char *id() const override
        {
            return id_;
        }
        void begin(Interpreter &interp, int32_t startZDu, int32_t startXDu) override
        {
            interp.begin(startZDu, startXDu);
        }
        const char *translate(Words &) override
        {
            return nullptr;
        }

    private:
        const char *id_;
    };

    Lathecode g_lathecode;
    Linuxcnc g_linuxcnc("linuxcnc");
    Linuxcnc g_ngc(ROUTINE_DIALECT);
    Dialect *const DIALECTS[] = {&g_lathecode, &g_linuxcnc, &g_ngc};
    constexpr size_t DIALECT_COUNT = sizeof(DIALECTS) / sizeof(DIALECTS[0]);
}

namespace Gcode
{
    Dialect *find(const char *id)
    {
        for (size_t i = 0; i < DIALECT_COUNT; i++) {
            if (strcmp(DIALECTS[i]->id(), id) == 0) {
                return DIALECTS[i];
            }
        }
        return nullptr;
    }

    size_t count()
    {
        return DIALECT_COUNT;
    }

    Dialect *at(size_t i)
    {
        return DIALECTS[i];
    }
}
