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
#ifndef ModeParamCodec_h
#define ModeParamCodec_h

#include "ModeInternal.hpp"

// Text form of a mode's parameter table: params is the mode's struct, the
// table gives each field's offset.
namespace Mode::Detail
{
    const Param *findParam(const Param *table, size_t count, const char *name);
    // The first required param still at 0, else nullptr.
    const Param *firstUnsetParam(const Param *table, size_t count, const uint8_t *params);
    bool paramIsSet(const Param &param, const uint8_t *params);
    // nullptr on success, else the reason; Text params are read-only.
    const char *parseParam(const Param &param, const char *value, uint8_t *params);
    // Sets every "name=value" of a describeParams() line, Text params too.
    // params is changed up to the first refusal.
    const char *loadParamLine(const Param *table, size_t count, const char *line, uint8_t *params);
    // "name=value ..." for every param.
    void describeParams(const Param *table, size_t count, const uint8_t *params, char *out, size_t outMax);
}

#endif
