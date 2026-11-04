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
#ifndef Status_h
#define Status_h

#include "Mode.hpp"
#include <stdint.h>

// One read of everything the display and web clients show.
namespace Status
{
    struct Axis
    {
        int64_t pos;
        bool moving;
        bool enabled;
    };

    struct Gear
    {
        int32_t pitchDu;
        const char *axis;
        int32_t stopAfterDu;
    };

    struct Snapshot
    {
        Axis z;
        Axis x;
        int32_t rpm;
        float angleDeg;
        const char *state; // Machine::stateName()
        const char *mode;
        bool running;
        bool ready; // all required params of the mode are set
        const char *units;
        int32_t jogStepDu;
        Gear gear;
        Mode::Progress job;
        int32_t feedOverridePct;
        const char *error; // why the last job ended on its own, "" if it finished
        char params[256];  // current mode's "name=value ..."
        bool probeContact;
    };

    Snapshot get();
}

#endif
