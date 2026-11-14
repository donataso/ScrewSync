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
#include "SwitchDebounce.hpp"

ZTEST_SUITE(switch_debounce, nullptr, nullptr, nullptr, nullptr, nullptr);

ZTEST(switch_debounce, test_press_after_samples)
{
    SwitchDebounce d = {false, 0};
    zassert_false(d.pressed(true, 3));
    zassert_false(d.pressed(true, 3));
    zassert_true(d.pressed(true, 3));
    zassert_false(d.pressed(true, 3));
}

ZTEST(switch_debounce, test_bounce_restarts_count)
{
    SwitchDebounce d = {false, 0};
    zassert_false(d.pressed(true, 3));
    zassert_false(d.pressed(true, 3));
    zassert_false(d.pressed(false, 3));
    zassert_false(d.pressed(true, 3));
    zassert_false(d.pressed(true, 3));
    zassert_true(d.pressed(true, 3));
}

ZTEST(switch_debounce, test_release_is_not_a_press)
{
    SwitchDebounce d = {true, 0};
    zassert_false(d.pressed(false, 2));
    zassert_false(d.pressed(false, 2));
    zassert_false(d.stable);
    zassert_false(d.pressed(true, 2));
    zassert_true(d.pressed(true, 2));
}
