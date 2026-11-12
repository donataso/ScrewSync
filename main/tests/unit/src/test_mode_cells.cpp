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
#include <zephyr/ztest.h>
#include <string.h>
#include "ModeCells.hpp"

using ModeCells::Cell;
using ModeCells::Kind;

namespace
{
    constexpr size_t COLUMNS = 2;
    constexpr size_t NARROW_COLUMNS = 3;
    constexpr size_t PICKER_SLOT = 15;

    bool hasValue(Kind kind)
    {
        return kind != Kind::Gap && kind != Kind::Picker;
    }

    template <typename Check>
    void forEachCell(Check check)
    {
        for (size_t id = 0; id < Mode::COUNT; id++) {
            size_t count = 0;
            const Cell *cells = ModeCells::cells((Mode::Id)id, count);
            for (size_t slot = 0; slot < count; slot++) {
                check(cells, count, slot);
            }
        }
    }
}

ZTEST_SUITE(mode_cells, NULL, NULL, NULL, NULL, NULL);

ZTEST(mode_cells, test_every_mode_has_a_label)
{
    for (size_t id = 0; id < Mode::COUNT; id++) {
        const char *label = ModeCells::label((Mode::Id)id);
        zassert_not_null(label);
        zassert_true(label[0] != '\0');
    }
}

ZTEST(mode_cells, test_cells_fit_the_panel)
{
    for (size_t id = 0; id < Mode::COUNT; id++) {
        size_t count = 0;
        const Cell *cells = ModeCells::cells((Mode::Id)id, count);
        zassert_true(count <= ModeCells::MAX);
        zassert_true(count == 0 || cells != nullptr);
    }
}

ZTEST(mode_cells, test_value_cells_have_param)
{
    forEachCell([](const Cell *cells, size_t, size_t slot) {
        const Cell &c = cells[slot];
        if (hasValue(c.kind)) {
            zassert_not_null(c.param);
        }
        if (c.kind == Kind::Toggle) {
            zassert_not_null(c.valueA);
            zassert_not_null(c.valueB);
            zassert_equal(c.labelA == nullptr, c.labelB == nullptr);
        } else if (c.kind != Kind::Gap) {
            zassert_not_null(c.label);
        }
    });
}

ZTEST(mode_cells, test_params_unique_within_a_mode)
{
    forEachCell([](const Cell *cells, size_t, size_t slot) {
        if (!cells[slot].param) {
            return;
        }
        for (size_t other = 0; other < slot; other++) {
            zassert_true(!cells[other].param || strcmp(cells[other].param, cells[slot].param) != 0, "param %s twice",
                         cells[slot].param);
        }
    });
}

ZTEST(mode_cells, test_picker_and_latch_slots)
{
    forEachCell([](const Cell *cells, size_t, size_t slot) {
        if (cells[slot].kind == Kind::Picker) {
            zassert_equal(slot, PICKER_SLOT);
        }
        if (cells[slot].kind == Kind::Latch) {
            zassert_true(slot >= ModeCells::NARROW_FIRST);
        }
    });
}

// A narrow row lies over one grid row; a mode uses one or the other.
ZTEST(mode_cells, test_narrow_rows_do_not_overlap_the_grid)
{
    forEachCell([](const Cell *cells, size_t, size_t slot) {
        if (slot < ModeCells::NARROW_FIRST || cells[slot].kind == Kind::Gap) {
            return;
        }
        size_t narrowRow = (slot - ModeCells::NARROW_FIRST) / NARROW_COLUMNS;
        size_t gridRowFirst = ModeCells::NARROW_FIRST - (2 - narrowRow) * COLUMNS;
        for (size_t i = gridRowFirst; i < gridRowFirst + COLUMNS; i++) {
            zassert_equal(cells[i].kind, Kind::Gap, "slot %zu overlaps slot %zu", slot, i);
        }
    });
}

ZTEST(mode_cells, test_when_names_a_param_of_the_mode)
{
    forEachCell([](const Cell *cells, size_t count, size_t slot) {
        const char *when = cells[slot].when;
        if (!when) {
            return;
        }
        const char *eq = strchr(when, '=');
        zassert_not_null(eq);
        zassert_true(eq[1] != '\0');
        bool found = false;
        for (size_t i = 0; i < count; i++) {
            const char *param = cells[i].param;
            found |= param && strlen(param) == (size_t)(eq - when) && !strncmp(param, when, eq - when);
        }
        zassert_true(found, "when %s names no param", when);
    });
}
