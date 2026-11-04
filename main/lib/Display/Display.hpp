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
#ifndef Display_h
#define Display_h

#include <stdint.h>
#include <stddef.h>

// The screen app on the Nextion display (docs/screen.md): status pushes,
// the mode panel and thread picker, and machine commands from its buttons.
namespace Display
{
    // Starts the link thread.
    void begin();

    // The display answers the link's pings.
    bool connected();

    // Image px = origin + mm * pxPerMm; Z across, X down. pxPerMm 0 = unknown.
    struct PreviewMapping
    {
        float originX;
        float originY;
        float pxPerMm;
    };

    // A baseline JPEG into the display's RAM for the gcode panel. Blocks the link for the transfer.
    bool sendPreview(const uint8_t *jpeg, size_t len, const PreviewMapping &mapping);
}

#endif
