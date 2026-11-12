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
#ifndef DisplayInternal_h
#define DisplayInternal_h

#include "Display.hpp"
#include "DisplayTheme.hpp"
#include "ModeCells.hpp"
#include "Nextion.hpp"
#include "Status.hpp"
#include <stddef.h>
#include <stdint.h>

namespace Display::detail
{
    using Nextion::KEY_MAX;

    void pushSelected(const char *component, bool selected);
    void showToast(const char *msg);

    bool inchUnits();
    const char *unitSuffix();
    // mm to 3 decimals, inch to 4, never "-0.000".
    void formatLength(double du, char *out, size_t outMax);
    void trimZeros(char *s);
    // Value of name in a "name=value ..." list.
    bool paramValue(const char *params, const char *name, char *out, size_t outMax);

    void pushStatus();
    void pushSoon();
    void clearPreview();

    void pushMenu(Mode::Id current);
    // Returns whether any cell's type changed.
    bool pushPanel(Mode::Id id, const char *params);

    void openPicker();
    void pushPicker();
    void selectPickerGroup(const char *group);
}

#endif
