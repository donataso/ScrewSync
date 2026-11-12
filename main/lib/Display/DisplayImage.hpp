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
#ifndef DisplayImage_h
#define DisplayImage_h

#include <stddef.h>
#include <stdint.h>

namespace DisplayImage
{
    struct Info
    {
        bool stored;
        uint32_t length;
        uint32_t crc;
    };

    Info info();

    // Exclusive right to flash the display; false while another flash runs.
    bool claim();
    void release();

    // claim() plus Machine flashing, for flash(). nullptr on success, else the reason.
    const char *beginFlash();
    void endFlash();

    // Caller holds beginFlash(). Flashes image; with store,
    // keeps it once the display reconnects running it. reply = outcome line.
    bool flash(const uint8_t *image, size_t len, bool store, const char *&reply);

    // Flashes the stored image in the background. nullptr if started, else
    // the reason.
    const char *flashStored();

    // Streams an image into the partition, holding claim() until finish or
    // abort. Each returns nullptr on success, else the reason.
    const char *storeBegin(uint32_t len, uint32_t crc);
    bool storeChunk(const uint8_t *data, size_t len);
    const char *storeFinish();
    void storeAbort();
}

#endif
