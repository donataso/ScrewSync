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
#include "InputSampler.hpp"
#include "Outputs.hpp"
#include "Pins.hpp"
#include "RunSettings.hpp"
#include <stdio.h>

using namespace Display::Theme;
using namespace Nextion;
using PinMap::Fn;

namespace
{
    struct PinRole
    {
        Fn fn;
        const char *label;
        bool output;
    };

    constexpr PinRole ROLES[] = {
        {Fn::Probe, "Probe", false},
        {Fn::ZHome, "Z home", false},
        {Fn::XHome, "X home", false},
        {Fn::Estop, "E-stop", false},
        {Fn::EstopOut, "E-stop out", true},
        {Fn::RunningOut, "Running out", true},
    };

    static_assert(sizeof(ROLES) / sizeof(ROLES[0]) <= Display::Theme::MENU_SLOTS, "pin cells exceed the cell grid");

    bool g_sampling = false;

    bool active(const PinRole &role, uint64_t levels)
    {
        if (role.output) {
            return Outputs::commanded(role.fn);
        }
        for (size_t nth = 0; nth < Pins::count(role.fn); nth++) {
            if (Pins::inputActive(role.fn, nth, levels)) {
                return true;
            }
        }
        return false;
    }

    void pushPinCell(size_t slot, const PinRole &role, uint64_t levels)
    {
        char key[KEY_MAX];
        snprintf(key, sizeof(key), "c%u.txt", (unsigned)slot);
        pushText(key, "%s", role.label);
        snprintf(key, sizeof(key), "c%u", (unsigned)slot);
        pushColor(key, "bco", active(role, levels) ? COLOR_START : COLOR_BUTTON);
        pushColor(key, "pco", COLOR_TEXT);
    }
}

namespace Display::Detail
{
    bool pinsOpen()
    {
        return RunSettings::pinsView();
    }

    void setPinsOpen(bool open)
    {
        if (open != RunSettings::pinsView()) {
            RunSettings::setPinsView(open);
            pushSoon();
        }
    }

    void pushPinsView()
    {
        bool open = RunSettings::pinsView();
        if (open != g_sampling) {
            g_sampling = open;
            if (open) {
                InputSampler::start();
            } else {
                InputSampler::stop();
            }
        }
        if (pushNum("pinsOpen", open)) {
            writeCommand("click pinsSync,1");
        }
    }

    bool pushPins()
    {
        uint64_t sampled;
        uint64_t levels;
        InputSampler::published(sampled, levels);
        size_t slot = 0;
        bool changed = false;
        for (const PinRole &role : ROLES) {
            if (Pins::count(role.fn) == 0) {
                continue;
            }
            pushPinCell(slot, role, levels);
            changed |= pushCellType(slot++, CellType::Pin);
        }
        for (; slot < ModeCells::MAX; slot++) {
            changed |= pushCellType(slot, CellType::Hidden);
        }
        return changed;
    }
}
