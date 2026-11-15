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
#ifndef GcodeInterpreter_h
#define GcodeInterpreter_h

#include "GcodeCore.hpp"

// Standard (LinuxCNC) interpreter: G0/G1/G33, G10 L20 P1, G20/G21, G90/G91,
// G94/G18/G8/G54 (no-op), M0/M1/M2/M30, G38.2-.5, M110 (MDI only),
// F, K, T (ignored). Z/X axes only, X is radius. G38.x is not modal.
namespace Gcode
{
    class Interpreter
    {
    public:
        enum class Motion : uint8_t
        {
            None,
            Rapid,
            Feed,
            Thread
        };

        void begin(int32_t startZDu, int32_t startXDu, Motion motion = Motion::None);
        const char *line(const Words &words, uint32_t lineNo, StepSink &sink);

        // Keeps the modal state.
        void setPosition(int32_t zDu, int32_t xDu)
        {
            zDu_ = zDu;
            xDu_ = xDu;
        }
        int32_t zDu() const
        {
            return zDu_;
        }
        int32_t xDu() const
        {
            return xDu_;
        }
        int32_t unitScaleDu() const
        {
            return unitScaleDu_;
        }
        void setMdi(bool mdi)
        {
            mdi_ = mdi;
        }

    private:
        Motion motion_;
        bool absolute_;
        int32_t unitScaleDu_;
        int32_t feedDuPerMin_;
        bool feedSet_;
        int32_t zDu_;
        int32_t xDu_;
        bool mdi_ = false;
    };
}

#endif
