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
#ifndef PanelPage_h
#define PanelPage_h

#include "Mode.hpp"
#include <stddef.h>

namespace Display::Detail
{
    struct PanelPage
    {
        Mode::Id mode = Mode::Id::Gear;
        size_t page = 0;

        void select(Mode::Id current, size_t pageCount, size_t requested)
        {
            page = requested < pageCount ? requested : 0;
            mode = current;
        }

        size_t shown(Mode::Id current) const
        {
            return mode == current ? page : 0;
        }

        void enter(Mode::Id current)
        {
            if (current != mode) {
                mode = current;
                page = 0;
            }
        }
    };
}

#endif
