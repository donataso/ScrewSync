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
#include "JogButton.hpp"

using Event = JogButton::Event;

ZTEST_SUITE(jog_button, nullptr, nullptr, nullptr, nullptr, nullptr);

ZTEST(jog_button, test_short_press_taps)
{
    JogButton b = {};
    zassert_equal(b.update(true, 0, 300, 100), Event::None);
    zassert_equal(b.update(true, 200, 300, 100), Event::None);
    zassert_equal(b.update(false, 250, 300, 100), Event::Tap);
    zassert_equal(b.update(false, 300, 300, 100), Event::None);
}

ZTEST(jog_button, test_long_press_holds_then_releases)
{
    JogButton b = {};
    b.update(true, 0, 300, 100);
    zassert_equal(b.update(true, 300, 300, 100), Event::Hold);
    zassert_equal(b.update(true, 350, 300, 100), Event::None);
    zassert_equal(b.update(true, 400, 300, 100), Event::Hold);
    zassert_equal(b.update(false, 420, 300, 100), Event::Release);
}

ZTEST(jog_button, test_blocked_until_release)
{
    JogButton b = {false, false, true, 0, 0};
    zassert_equal(b.update(true, 0, 300, 100), Event::None);
    zassert_equal(b.update(true, 500, 300, 100), Event::None);
    zassert_equal(b.update(false, 600, 300, 100), Event::None);
    zassert_equal(b.update(true, 700, 300, 100), Event::None);
    zassert_equal(b.update(false, 750, 300, 100), Event::Tap);
}
