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
#include "InputSubscribers.hpp"

using Change = InputSubscribers::Change;

ZTEST_SUITE(input_subscribers, nullptr, nullptr, nullptr, nullptr, nullptr);

ZTEST(input_subscribers, test_first_starts_last_stops)
{
    InputSubscribers subs;
    zassert_equal(subs.set(2, true), Change::Started);
    zassert_equal(subs.set(5, true), Change::None);
    zassert_true(subs.has(2));
    zassert_true(subs.has(5));
    zassert_equal(subs.set(2, false), Change::None);
    zassert_false(subs.has(2));
    zassert_equal(subs.set(5, false), Change::Stopped);
}

ZTEST(input_subscribers, test_repeat_changes_nothing)
{
    InputSubscribers subs;
    zassert_equal(subs.set(1, false), Change::None);
    zassert_equal(subs.set(1, true), Change::Started);
    zassert_equal(subs.set(1, true), Change::None);
    zassert_equal(subs.set(1, false), Change::Stopped);
    zassert_equal(subs.set(1, false), Change::None);
}
