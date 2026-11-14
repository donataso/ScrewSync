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
#ifndef PinMapJson_h
#define PinMapJson_h

#include "PinMap.hpp"

// GET /pins: {"board":{name,pcntUnits,maxHwFilterUs,maxSwFilterUs,tickUs,ppr:[{fn,ppr}],strapping,reserved:[{gpio,note}],
//   connectors:[{id,title,row,col,pins}]},"functions":[...],"names":[...]}
// POST /pins: {"functions":[{id,connector,start,encoder,filterUs,signals:[{invert,pull}]}],
//   "names":[{connector,name}]}
// Pins: GPIO number, -1 = 5V, -2 = GND. encoder: "hw", "sw" or "".
// filterUs: PCNT glitch filter (hw) or debounce time (sw); missing = the function's default.
// A non-exclusive function can be in "functions" more than once.
// GET /pins/functions: {"functions":[{id,label,group,signals,dir,required,exclusive,activeLow,encoders,filterUs,maxRpm}]}
namespace PinMapJson
{
    int encode(const PinMap::Board &board, const PinMap::Map &map, char *out, size_t outMax);

    // Builds and validates out; nullptr on success, else the reason (in err).
    const char *decode(const PinMap::Board &board, char *json, size_t len, PinMap::Map &out, char *err,
                       size_t errMax);

    int encodeFunctions(char *out, size_t outMax);
}

#endif
