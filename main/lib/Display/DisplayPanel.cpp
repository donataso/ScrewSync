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
#include "PanelPage.hpp"
#include "Settings.hpp"
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
    PanelPage g_panelPage;

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

    bool pushLatchCell(size_t slot, const char *text, int color, const char *command)
    {
        char key[KEY_MAX];
        snprintf(key, sizeof(key), "c%u.txt", (unsigned)slot);
        pushText(key, "%s", text);
        snprintf(key, sizeof(key), "c%u", (unsigned)slot);
        pushColor(key, "bco", color);
        pushColor(key, "borderc", color);
        snprintf(key, sizeof(key), "cCmd%u.txt", (unsigned)slot);
        pushText(key, "%s", command);
        return pushCellType(slot, CellType::Latch);
    }

    bool pushLatch(size_t slot, const char *mode, const Cell &c, const char *raw)
    {
        bool on = !strcmp(raw, "on");
        char command[48];
        snprintf(command, sizeof(command), "%s %s %s", mode, c.param, on ? "off" : "on");
        return pushLatchCell(slot, c.label, on ? COLOR_SELECTED : COLOR_BUTTON, command);
    }

    bool pushCycle(size_t slot, const char *mode, const Cell &c, const char *raw)
    {
        size_t index = listIndex(c.valueA, raw);
        char next[16];
        if (!listItem(c.valueA, index + 1, next, sizeof(next))) {
            listItem(c.valueA, 0, next, sizeof(next));
        }
        char shown[16];
        listItem(c.labelA, index, shown, sizeof(shown));
        char text[32];
        snprintf(text, sizeof(text), "%s: %s", c.label, shown);
        char command[48];
        snprintf(command, sizeof(command), "%s %s %s", mode, c.param, next);
        return pushLatchCell(slot, text, index != 0 ? COLOR_SELECTED : COLOR_BUTTON, command);
    }

    bool pushValue(size_t slot, Mode::Id id, const Cell &c, const char *raw)
    {
        char text[32];
        bool set = formatCellValue(c, raw, inchUnits(), text, sizeof(text));
        bool probe = id == Mode::Id::Probe;
        bool required = probe ? !strcmp(c.param, "probeDstDu") : Mode::required(id, c.param);
        char key[KEY_MAX];
        snprintf(key, sizeof(key), "c%u.txt", (unsigned)slot);
        pushText(key, "%s: %s", c.label, set || required ? text : "off");
        snprintf(key, sizeof(key), "c%u", (unsigned)slot);
        pushColor(key, "pco", set || !required ? COLOR_TEXT : COLOR_NOT_SET);
        pushColor(key, "bco", COLOR_BUTTON);
        pushColor(key, "borderc", COLOR_BUTTON);
        snprintf(key, sizeof(key), "cCmd%u.txt", (unsigned)slot);
        pushText(key, "%s %s ", probe ? "settings set" : Mode::name(id), c.param);
        return pushCellType(slot, valueType(c.kind));
    }

    // nullptr labels: arrows along the mode's axis.
    int toggleLabels(const Cell &c, const char *params, const char *labels[2])
    {
        if (c.labelA) {
            labels[0] = c.labelA;
            labels[1] = c.labelB;
            return FONT_LABEL;
        }
        char axis[4];
        bool onX = paramValue(params, "axis", axis, sizeof(axis)) && !strcmp(axis, "x");
        labels[0] = onX ? "↓" : "←";
        labels[1] = onX ? "↑" : "→";
        return FONT_SYMBOLS;
    }

    void pushHalf(size_t slot, const char *side, const char *text, int font)
    {
        char key[KEY_MAX];
        snprintf(key, sizeof(key), "c%u%s.txt", (unsigned)slot, side);
        pushText(key, "%s", text);
        snprintf(key, sizeof(key), "c%u%s.font", (unsigned)slot, side);
        pushNum(key, font);
    }

    bool pushToggle(size_t slot, const char *mode, const Cell &c, const char *raw, const char *params)
    {
        const char *labels[2];
        int font = toggleLabels(c, params, labels);
        char key[KEY_MAX];
        snprintf(key, sizeof(key), "cCmd%u.txt", (unsigned)slot);
        pushText(key, "%s %s %s|%s %s %s", mode, c.param, c.valueA, mode, c.param, c.valueB);
        static const char *const SIDES[] = {"a", "b"};
        const char *values[] = {c.valueA, c.valueB};
        for (size_t i = 0; i < 2; i++) {
            pushHalf(slot, SIDES[i], labels[i], font);
            snprintf(key, sizeof(key), "c%u%s", (unsigned)slot, SIDES[i]);
            pushSelected(key, !strcmp(raw, values[i]));
        }
        return pushCellType(slot, CellType::Toggle);
    }

    // Toggle: shows the current value. Latch: shows its label, lit while on. The command flips it.
    bool pushFlip(size_t slot, const char *side, const char *mode, const Cell &c, const char *params,
                  char *command, size_t commandMax)
    {
        char raw[24];
        if (!c.param || !paramValue(params, c.param, raw, sizeof(raw))) {
            return false;
        }
        char key[KEY_MAX];
        snprintf(key, sizeof(key), "c%u%s", (unsigned)slot, side);
        if (c.kind == Kind::Latch) {
            bool on = !strcmp(raw, "on");
            pushHalf(slot, side, c.label, FONT_LABEL);
            pushSelected(key, on);
            snprintf(command, commandMax, "%s %s %s", mode, c.param, on ? "off" : "on");
            return true;
        }
        const char *labels[2];
        int font = toggleLabels(c, params, labels);
        bool onB = !strcmp(raw, c.valueB);
        pushHalf(slot, side, labels[onB], font);
        pushSelected(key, false);
        snprintf(command, commandMax, "%s %s %s", mode, c.param, onB ? c.valueA : c.valueB);
        return true;
    }

    bool pushPair(size_t slot, const char *mode, const Cell &c, const char *params)
    {
        char left[48];
        char right[48] = "";
        if (!pushFlip(slot, "a", mode, *c.left, params, left, sizeof(left))) {
            return pushCellType(slot, CellType::Hidden);
        }
        bool both = c.right && pushFlip(slot, "b", mode, *c.right, params, right, sizeof(right));
        char key[KEY_MAX];
        snprintf(key, sizeof(key), "cCmd%u.txt", (unsigned)slot);
        pushText(key, "%s|%s", left, right);
        return pushCellType(slot, both ? CellType::Toggle : CellType::Half);
    }

    bool pushCell(size_t slot, Mode::Id id, const Cell &c, const char *params)
    {
        if (c.kind == Kind::Button) {
            return pushLatchCell(slot, c.label, COLOR_SETTINGS, c.valueA);
        }
        if (c.kind == Kind::Picker) {
            if (slot != PICKER_SLOT || !cellShown(c, params)) {
                return pushCellType(slot, CellType::Hidden);
            }
            pushText("picker.txt", "%s", c.label);
            return pushCellType(slot, CellType::Picker);
        }
        if (c.kind == Kind::Pair) {
            return pushPair(slot, Mode::name(id), c, params);
        }
        char raw[24];
        if (c.kind == Kind::Gap || !cellShown(c, params) || !paramValue(params, c.param, raw, sizeof(raw))) {
            return pushCellType(slot, CellType::Hidden);
        }
        const char *mode = Mode::name(id);
        if (c.kind == Kind::Latch) {
            return pushLatch(slot, mode, c, raw);
        }
        if (c.kind == Kind::Cycle) {
            return pushCycle(slot, mode, c, raw);
        }
        if (c.kind == Kind::Toggle) {
            return pushToggle(slot, mode, c, raw, params);
        }
        return pushValue(slot, id, c, raw);
    }
}

namespace Display::Detail
{
    void setPanelPage(size_t page)
    {
        Mode::Id id = Mode::current();
        g_panelPage.select(id, ModeCells::pages(id), page);
        pushSoon();
    }

    size_t panelPage()
    {
        return g_panelPage.shown(Mode::current());
    }

    bool pushCellType(size_t slot, CellType type)
    {
        char key[KEY_MAX];
        snprintf(key, sizeof(key), "cKind%u", (unsigned)slot);
        return pushNum(key, (int)type);
    }

    bool pushPanel(Mode::Id id, const char *params)
    {
        char probeParams[64];
        if (id == Mode::Id::Probe) {
            snprintf(probeParams, sizeof(probeParams), "probeDstDu=%ld probeOffsetDu=%ld",
                     (long)Settings::g_active.probeDstDu, (long)Settings::g_active.probeOffsetDu);
            params = probeParams;
        }
        size_t count;
        g_panelPage.enter(id);
        const Cell *cells = ModeCells::cells(id, g_panelPage.page, count);
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
        size_t next = 0;
        for (size_t i = 0; i < MENU_SLOTS; i++) {
            while (next < Mode::COUNT && Mode::unavailable((Mode::Id)next)) {
                next++;
            }
            bool used = next < Mode::COUNT;
            Mode::Id id = (Mode::Id)next;
            snprintf(key, sizeof(key), "menu%u.txt", (unsigned)i);
            pushText(key, "%s", used ? ModeCells::label(id) : "");
            snprintf(key, sizeof(key), "menuCmd%u.txt", (unsigned)i);
            pushText(key, "%s", used ? Mode::name(id) : "");
            snprintf(key, sizeof(key), "menu%u", (unsigned)i);
            bool selected = used && id == current;
            bool probe = used && id == Mode::Id::Probe;
            int color = COLOR_BUTTON;
            if (selected) {
                color = COLOR_SELECTED;
            } else if (probe) {
                color = COLOR_SETTINGS;
            }
            pushColor(key, "bco", color);
            next += used;
        }
    }
}
