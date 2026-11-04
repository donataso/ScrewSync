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

// Flavor for https://kachurovskiy.com/lathecode/ output: G0/G1/G32,
// G20/G21, G90/G91, G94/G18 (no-op), M0/M1/M2/M30, F, T. Z/X axes only,
// X is radius from the centreline.
namespace Gcode
{
    class Lathecode : public Flavor
    {
    public:
        void begin(int32_t startZDu, int32_t startXDu) override;
        const char *line(const Words &words, uint32_t lineNo, StepSink &sink) override;
        const char *id() const override
        {
            return "lathecode";
        }

    private:
        enum class Motion
        {
            Rapid,
            Feed,
            Thread
        };

        Motion motion_;
        bool absolute_;
        int32_t unitScaleDu_;
        int32_t feedDuPerMin_;
        bool feedSet_;
        int32_t threadLeadDu_;
        bool threadLeadSet_;
        int32_t zDu_;
        int32_t xDu_;
    };
}

#endif
