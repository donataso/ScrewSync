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
#include "DisplayInternal.hpp"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

using namespace Display::Detail;
using namespace Display::Theme;
using namespace Nextion;
using ModeCells::Cell;
using ModeCells::Kind;

namespace
{
    static_assert(Mode::COUNT <= MENU_SLOTS, "mode menu too small");

    CellType valueType(Kind kind)
    {
        switch (kind) {
        case Kind::Pitch:
            return CellType::Pitch;
        case Kind::Count:
            return CellType::Count;
        case Kind::Angle:
            return CellType::Angle;
        default:
            return CellType::Length;
        }
    }

    // Returns whether the cell's type changed.
    bool pushCellType(size_t slot, CellType type)
    {
        char key[KEY_MAX];
        snprintf(key, sizeof(key), "cKind%u.val", (unsigned)slot);
        return pushNum(key, (int)type);
    }

    bool pushLatch(size_t slot, const char *mode, const Cell &c, const char *raw)
    {
        bool on = !strcmp(raw, "on");
        char key[KEY_MAX];
        snprintf(key, sizeof(key), "c%u.txt", (unsigned)slot);
        pushText(key, "%s", c.label);
        snprintf(key, sizeof(key), "c%u", (unsigned)slot);
        pushSelected(key, on);
        pushColor(key, "borderc", on ? COLOR_SELECTED : COLOR_BUTTON);
        snprintf(key, sizeof(key), "cCmd%u.txt", (unsigned)slot);
        pushText(key, "%s %s %s", mode, c.param, on ? "off" : "on");
        return pushCellType(slot, CellType::Latch);
    }

    bool pushValue(size_t slot, Mode::Id id, const Cell &c, const char *raw)
    {
        char text[32];
        bool set = formatCellValue(c, raw, inchUnits(), text, sizeof(text));
        bool required = Mode::required(id, c.param);
        char key[KEY_MAX];
        snprintf(key, sizeof(key), "c%u.txt", (unsigned)slot);
        pushText(key, "%s: %s", c.label, set || required ? text : "off");
        snprintf(key, sizeof(key), "c%u", (unsigned)slot);
        pushColor(key, "pco", set || !required ? COLOR_TEXT : COLOR_NOT_SET);
        snprintf(key, sizeof(key), "cCmd%u.txt", (unsigned)slot);
        pushText(key, "%s %s ", Mode::name(id), c.param);
        return pushCellType(slot, valueType(c.kind));
    }

    bool pushToggle(size_t slot, const char *mode, const Cell &c, const char *raw, const char *params)
    {
        const char *labels[] = {c.labelA, c.labelB};
        int font = FONT_LABEL;
        if (!c.labelA) {
            char axis[4];
            bool onX = paramValue(params, "axis", axis, sizeof(axis)) && !strcmp(axis, "x");
            labels[0] = onX ? "↓" : "←";
            labels[1] = onX ? "↑" : "→";
            font = FONT_SYMBOLS;
        }
        char key[KEY_MAX];
        snprintf(key, sizeof(key), "cCmd%u.txt", (unsigned)slot);
        pushText(key, "%s %s %s|%s %s %s", mode, c.param, c.valueA, mode, c.param, c.valueB);
        static const char *const SIDES[] = {"a", "b"};
        const char *values[] = {c.valueA, c.valueB};
        for (size_t i = 0; i < 2; i++) {
            snprintf(key, sizeof(key), "c%u%s.txt", (unsigned)slot, SIDES[i]);
            pushText(key, "%s", labels[i]);
            snprintf(key, sizeof(key), "c%u%s.font", (unsigned)slot, SIDES[i]);
            pushNum(key, font);
            snprintf(key, sizeof(key), "c%u%s", (unsigned)slot, SIDES[i]);
            pushSelected(key, !strcmp(raw, values[i]));
        }
        return pushCellType(slot, CellType::Toggle);
    }

    bool pushCell(size_t slot, Mode::Id id, const Cell &c, const char *params)
    {
        if (c.kind == Kind::Picker) {
            if (slot != PICKER_SLOT || !cellShown(c, params)) {
                return pushCellType(slot, CellType::Hidden);
            }
            pushText("picker.txt", "%s", c.label);
            return pushCellType(slot, CellType::Picker);
        }
        char raw[24];
        if (c.kind == Kind::Gap || !cellShown(c, params) || !paramValue(params, c.param, raw, sizeof(raw))) {
            return pushCellType(slot, CellType::Hidden);
        }
        const char *mode = Mode::name(id);
        if (c.kind == Kind::Latch) {
            return pushLatch(slot, mode, c, raw);
        }
        if (c.kind == Kind::Toggle) {
            return pushToggle(slot, mode, c, raw, params);
        }
        return pushValue(slot, id, c, raw);
    }
}

namespace Display::Detail
{
    bool pushPanel(Mode::Id id, const char *params)
    {
        size_t count;
        const Cell *cells = ModeCells::cells(id, count);
        bool changed = false;
        for (size_t slot = 0; slot < ModeCells::MAX; slot++) {
            if (slot < count) {
                changed |= pushCell(slot, id, cells[slot], params);
            } else {
                changed |= pushCellType(slot, CellType::Hidden);
            }
        }
        return changed;
    }

    void pushMenu(Mode::Id current)
    {
        char key[KEY_MAX];
        for (size_t i = 0; i < MENU_SLOTS; i++) {
            bool used = i < Mode::COUNT;
            snprintf(key, sizeof(key), "menu%u.txt", (unsigned)i);
            pushText(key, "%s", used ? ModeCells::label((Mode::Id)i) : "");
            snprintf(key, sizeof(key), "menuCmd%u.txt", (unsigned)i);
            pushText(key, "%s", used ? Mode::name((Mode::Id)i) : "");
            snprintf(key, sizeof(key), "menu%u", (unsigned)i);
            bool selected = used && i == (size_t)current;
            bool probe = used && (Mode::Id)i == Mode::Id::Probe;
            int color = COLOR_BUTTON;
            if (selected) {
                color = COLOR_SELECTED;
            } else if (probe) {
                color = COLOR_SETTINGS;
            }
            pushColor(key, "bco", color);
        }
    }
}
