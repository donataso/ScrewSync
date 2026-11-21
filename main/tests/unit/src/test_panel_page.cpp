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
#include "PanelPage.hpp"

using Display::Detail::PanelPage;
using Mode::Id;

ZTEST_SUITE(panel_page, NULL, NULL, NULL, NULL, NULL);

ZTEST(panel_page, test_select_in_range_is_shown)
{
    PanelPage panel;
    panel.select(Id::Gear, 2, 1);
    zassert_equal(panel.shown(Id::Gear), 1);
}

ZTEST(panel_page, test_select_out_of_range_resets_to_first)
{
    PanelPage panel;
    panel.select(Id::Gear, 2, 2);
    zassert_equal(panel.shown(Id::Gear), 0);
    panel.select(Id::Gear, 1, 1);
    zassert_equal(panel.shown(Id::Gear), 0);
}

ZTEST(panel_page, test_other_mode_reports_first_page)
{
    PanelPage panel;
    panel.select(Id::Gear, 2, 1);
    zassert_equal(panel.shown(Id::Probe), 0);
}

ZTEST(panel_page, test_enter_same_mode_keeps_page)
{
    PanelPage panel;
    panel.select(Id::Gear, 2, 1);
    panel.enter(Id::Gear);
    zassert_equal(panel.shown(Id::Gear), 1);
}

ZTEST(panel_page, test_enter_new_mode_resets_page)
{
    PanelPage panel;
    panel.select(Id::Gear, 2, 1);
    panel.enter(Id::Probe);
    zassert_equal(panel.page, 0);
    panel.enter(Id::Gear);
    zassert_equal(panel.shown(Id::Gear), 0);
}
