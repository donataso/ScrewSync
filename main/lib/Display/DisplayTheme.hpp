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
#ifndef DisplayTheme_h
#define DisplayTheme_h

#include "ThreadTable.hpp"
#include <stddef.h>
#include <stdint.h>

// Values shared with display/ and scripts/screenshots.py.
namespace Display::theme
{
    // RGB565, matching display/pages/page0.page.toml.
    constexpr int COLOR_SELECTED = 7037;
    constexpr int COLOR_BUTTON = 8485;
    constexpr int COLOR_BUTTON_PRESSED = 12711;
    constexpr int COLOR_START = 9254;
    constexpr int COLOR_STOP = 45316;
    constexpr int COLOR_TEXT = 59262;
    constexpr int COLOR_DIM = 36019;
    constexpr int COLOR_AXIS_OFF = 14824;
    constexpr int COLOR_Z = 2927;
    constexpr int COLOR_Z_PRESSED = 5203;
    constexpr int COLOR_X = 27124;
    constexpr int COLOR_X_PRESSED = 33432;
    constexpr int COLOR_ZERO = 45763;
    constexpr int COLOR_ZERO_PRESSED = 52069;
    constexpr int COLOR_NOT_SET = 64138;
    constexpr int COLOR_SETTINGS = 10829; // matches menuSettings's bco

    // display/app.toml font order; arrows exist only in the symbols font.
    constexpr int FONT_LABEL = 0;
    constexpr int FONT_SYMBOLS = 2;

    // Mode panel cell types (cKindN.val), matching display/pages/page0.nx.
    // Value types pick the keypad's conversion; the cell's command gets the
    // typed value appended. Toggle cells hold two commands, "a|b".
    enum class CellType : int
    {
        Hidden = 0,
        Length = 1,
        Pitch = 2,
        Count = 3,
        Angle = 4,
        Toggle = 5,
        Picker = 6,
        Latch = 7,
    };

    // Page ids, display/app.toml order.
    constexpr uint8_t PAGE_MAIN = 0;
    constexpr uint8_t PAGE_THREADS = 1;

    constexpr size_t MENU_SLOTS = 10;
    constexpr size_t PICKER_TABS = ThreadTable::MAX_GROUPS;
    constexpr size_t PICKER_SLOT = 15; // "picker" button in the last narrow cell
    constexpr size_t PICKER_CELLS = ThreadTable::MAX_GROUP_ENTRIES;
    constexpr size_t TOAST_MAX_CHARS = 60; // page0 toast.txt_maxl

    // Must match gcodePic and gcodeTool in display/pages/page0.page.toml.
    constexpr char PREVIEW_PATH[] = "ram/preview.jpg";
    constexpr int PREVIEW_X = 320;
    constexpr int PREVIEW_Y = 80;
    constexpr int PREVIEW_W = 464;
    constexpr int PREVIEW_H = 288;
    constexpr int TOOL_SIZE = 8;
}

#endif
