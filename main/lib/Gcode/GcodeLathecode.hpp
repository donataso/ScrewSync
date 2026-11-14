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
#ifndef GcodeLathecode_h
#define GcodeLathecode_h

#include "GcodeCore.hpp"

// Dialect for https://kachurovskiy.com/lathecode/ output: the standard set
// with G32 instead of G33, its F as the modal thread lead. Starts in G1.
namespace Gcode
{
    class Lathecode : public Dialect
    {
    public:
        const char *id() const override
        {
            return "lathecode";
        }
        void begin(Interpreter &interp, int32_t startZDu, int32_t startXDu) override;
        const char *translate(Words &words) override;

    private:
        bool thread_;
        bool leadSet_;
        double leadDu_;
        int32_t unitScaleDu_;
    };
}

#endif
