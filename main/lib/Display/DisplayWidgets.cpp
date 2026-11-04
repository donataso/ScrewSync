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

namespace Display::detail
{
    void pushSelected(const char *component, bool selected)
    {
        Nextion::pushColor(component, "bco", selected ? theme::COLOR_SELECTED : theme::COLOR_BUTTON);
    }

    void showToast(const char *msg)
    {
        char text[theme::TOAST_MAX_CHARS + 1];
        size_t n = 0;
        for (; *msg && n < theme::TOAST_MAX_CHARS; msg++) {
            if (*msg >= 32 && *msg <= 126) {
                text[n++] = Nextion::safeChar(*msg);
            }
        }
        text[n] = '\0';

        char cmd[theme::TOAST_MAX_CHARS + 16];
        snprintf(cmd, sizeof(cmd), "toast.txt=\"%s\"", text);
        Nextion::writeCommand(cmd);
        Nextion::writeCommand("click toastShow,1");
    }
}
