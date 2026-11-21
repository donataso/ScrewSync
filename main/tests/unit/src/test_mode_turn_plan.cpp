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
#include "ModeTurnPlan.hpp"
#include <string.h>

using namespace Mode::Detail;

ZTEST_SUITE(mode_turn_plan, NULL, NULL, NULL, NULL, NULL);

ZTEST(mode_turn_plan, test_no_finish_ignores_finish_feed)
{
    zassert_is_null(turnFinishError(0, 1000, 0));
    zassert_is_null(turnFinishError(0, 1000, 50));
}

ZTEST(mode_turn_plan, test_finish_below_depth_with_feed_is_valid)
{
    zassert_is_null(turnFinishError(200, 1000, 50));
}

ZTEST(mode_turn_plan, test_finish_at_or_above_depth_refused)
{
    zassert_str_equal(turnFinishError(1000, 1000, 50), "finish must be below depth");
    zassert_str_equal(turnFinishError(1500, 1000, 50), "finish must be below depth");
}

ZTEST(mode_turn_plan, test_finish_without_feed_refused)
{
    zassert_str_equal(turnFinishError(200, 1000, 0), "set finish-feed first");
}

ZTEST(mode_turn_plan, test_depth_error_wins_over_missing_feed)
{
    zassert_str_equal(turnFinishError(1000, 1000, 0), "finish must be below depth");
}

ZTEST(mode_turn_plan, test_finish_levels_count_finish_and_spring)
{
    zassert_equal(turnFinishLevels(0, false), 0);
    zassert_equal(turnFinishLevels(200, false), 1);
    zassert_equal(turnFinishLevels(0, true), 1);
    zassert_equal(turnFinishLevels(200, true), 2);
}
