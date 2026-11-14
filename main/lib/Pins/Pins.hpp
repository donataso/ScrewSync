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
#ifndef Pins_h
#define Pins_h

#include "PinMap.hpp"
#include <stddef.h>
#include <zephyr/drivers/gpio.h>

// The active pin map: the valid assignments of the saved map, else the board defaults.
// A saved change applies after a reboot.
namespace Pins
{
    // Call before the modules that use pins. Also routes the PCNT inputs.
    void begin();

    // Number of assignments of fn.
    size_t count(PinMap::Fn fn);
    // False if the nth assignment of fn does not exist or has no pins.
    bool spec(PinMap::Fn fn, size_t nth, size_t signal, gpio_dt_spec &out);
    bool spec(PinMap::Fn fn, size_t signal, gpio_dt_spec &out);
    PinMap::Encoder encoder(PinMap::Fn fn);
    int pcntUnit(PinMap::Fn fn);

    bool usingDefaults();

    // The saved map (not the active one) with the board.
    int toJson(char *out, size_t outMax);
    // Validates and saves; nullptr on success, else the reason.
    const char *fromJson(char *json, size_t len);
    // Saves the board defaults; nullptr on success.
    const char *reset();
}

#endif
