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
#ifndef Ota_h
#define Ota_h

#include <stdint.h>
#include <stddef.h>

// MCUboot-backed OTA (WebUpdate.cpp's POST /update streams into this). The
// image lands in the inactive slot and boots in MCUboot's "test" mode:
// selfTestTick() confirms it permanent after OTA_SELFTEST_MS of clean
// operation, otherwise MCUboot itself reverts to the previous slot.
namespace Ota
{
    void begin(); // call once at startup, before selfTestTick()

    // Streams one chunk into the inactive slot; false aborts the upload.
    bool writeChunk(const uint8_t *data, size_t len);

    // Flushes + requests the test-mode upgrade; true means the caller should
    // reboot (Ota doesn't reboot itself -- the HTTP route needs to answer first).
    bool finish();

    void abort(); // discards an in-progress upload (bad chunk, client gave up)

    void selfTestTick(uint32_t nowMs);

    bool isActive(); // diagnostic -- true between the first writeChunk() and finish()/abort()
}

#endif
