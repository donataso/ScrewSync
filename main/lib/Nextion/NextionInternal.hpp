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
#ifndef NextionInternal_h
#define NextionInternal_h

#include "Nextion.hpp"

namespace Nextion::Detail
{
    const Config &config();
    void writeBytes(const uint8_t *data, size_t len);
    void writeTerminator();
    void drainInput();
    bool setBaud(uint32_t baud);
    bool readByte(uint8_t &out, uint32_t timeoutMs);
    bool waitForByte(uint8_t expected, uint32_t timeoutMs);
    // Unlike pause(), takes the UART even from another transfer.
    void seize();
    void release(bool restarted);
}

#endif
