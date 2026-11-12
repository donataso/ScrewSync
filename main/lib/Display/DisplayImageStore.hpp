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
#ifndef DisplayImageStore_h
#define DisplayImageStore_h

#include <stddef.h>
#include <stdint.h>

// The "display" partition's layout; must match scripts/pack_display_image.py.
namespace DisplayImage::Detail
{
    // Keeps image unless already stored; returns the outcome line.
    const char *storeImage(const uint8_t *image, size_t len);

    bool readStored(bool (*onLength)(uint32_t length), bool (*onChunk)(const uint8_t *data, size_t len));
}

#endif
