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
#include "ModePassPlan.hpp"

using namespace Mode::Detail;

namespace
{
    PassPlan makePlan(int32_t levels, int32_t finishLevels = 0, int32_t starts = 1)
    {
        return {levels, finishLevels, starts, false, 1000, 5000, 0, 0};
    }
}

ZTEST_SUITE(mode_pass_plan, NULL, NULL, NULL, NULL, NULL);

ZTEST(mode_pass_plan, test_linear_depth_steps_evenly)
{
    PassPlan plan = makePlan(4, 1);
    zassert_within(passDepthDu(plan, 1), 250, 1e-9);
    zassert_within(passDepthDu(plan, 2), 500, 1e-9);
    zassert_within(passDepthDu(plan, 4), 1000, 1e-9);
    // Finish pass stays at full depth.
    zassert_within(passDepthDu(plan, 5), 1000, 1e-9);
}

ZTEST(mode_pass_plan, test_squared_depth_cuts_deeper_first)
{
    PassPlan plan = makePlan(4);
    plan.squaredDepth = true;
    zassert_within(passDepthFraction(plan, 1), 0.4375, 1e-9);
    zassert_within(passDepthFraction(plan, 4), 1, 1e-9);
    double previousCut = passDepthFraction(plan, 1);
    for (int32_t level = 2; level <= 4; level++) {
        double cut = passDepthFraction(plan, level) - passDepthFraction(plan, level - 1);
        zassert_true(cut > 0 && cut < previousCut);
        previousCut = cut;
    }
}

ZTEST(mode_pass_plan, test_length_shortens_each_level)
{
    PassPlan plan = makePlan(4, 2);
    plan.lengthDu = 1000;
    plan.endReductionDu = 100;
    zassert_within(passLengthDu(plan, 1), 1000, 1e-9);
    zassert_within(passLengthDu(plan, 2), 900, 1e-9);
    zassert_within(passLengthDu(plan, 4), 700, 1e-9);
    zassert_within(passLengthDu(plan, 6), 700, 1e-9);
    plan.endReductionDu = 600;
    zassert_within(passLengthDu(plan, 3), 0, 1e-9);
}

ZTEST(mode_pass_plan, test_no_flank_offset_when_radial)
{
    PassPlan plan = makePlan(4);
    for (int32_t level = 0; level <= 5; level++) {
        zassert_within(passFlankOffsetDu(plan, level), 0, 1e-9);
    }
}

ZTEST(mode_pass_plan, test_flank_offset_alternates_sides)
{
    PassPlan plan = makePlan(4, 1);
    plan.flankTan = 0.5;
    // Each level is 250 deeper: a 125 shift, one side then the other.
    zassert_within(passFlankOffsetDu(plan, 1), 0, 1e-9);
    zassert_within(passFlankOffsetDu(plan, 2), 125, 1e-9);
    zassert_within(passFlankOffsetDu(plan, 3), 0, 1e-9);
    zassert_within(passFlankOffsetDu(plan, 4), 125, 1e-9);
    zassert_within(passFlankOffsetDu(plan, 5), 125, 1e-9);
}

ZTEST(mode_pass_plan, test_runs_every_level_once)
{
    PassPlan plan = makePlan(3, 1);
    PassCursor cursor = {0, 0, 0};
    zassert_equal(passNumber(plan, cursor), 0);
    int32_t passes = 0;
    while (true) {
        int32_t expected = passNextLevel(plan, cursor);
        if (!passAdvance(plan, cursor)) {
            break;
        }
        passes++;
        zassert_equal(cursor.level, expected);
        zassert_equal(cursor.level, passes);
        zassert_equal(passNumber(plan, cursor), passes);
    }
    zassert_equal(passes, 4);
    zassert_equal(passCount(plan), 4);
    zassert_equal(passNumber(plan, cursor), 4);
}

ZTEST(mode_pass_plan, test_runs_every_start_per_level)
{
    PassPlan plan = makePlan(2, 0, 3);
    PassCursor cursor = {0, 0, 0};
    int32_t pass = 0;
    for (int32_t level = 1; level <= 2; level++) {
        for (int32_t start = 0; start < 3; start++) {
            zassert_equal(passNextLevel(plan, cursor), level);
            zassert_true(passAdvance(plan, cursor));
            zassert_equal(cursor.level, level);
            zassert_equal(cursor.startIndex, start);
            zassert_equal(passNumber(plan, cursor), ++pass);
        }
    }
    zassert_false(passAdvance(plan, cursor));
    zassert_equal(passCount(plan), 6);
}

ZTEST(mode_pass_plan, test_skip_jumps_the_next_level)
{
    PassPlan plan = makePlan(5);
    PassCursor cursor = {0, 0, 0};
    passSkipLevel(plan, cursor);
    zassert_equal(cursor.skipLevels, 0, "nothing to skip before the first pass");
    passAdvance(plan, cursor);
    passSkipLevel(plan, cursor);
    zassert_equal(passNextLevel(plan, cursor), 3);
    zassert_true(passAdvance(plan, cursor));
    zassert_equal(cursor.level, 3);
    zassert_equal(cursor.skipLevels, 0);
}

ZTEST(mode_pass_plan, test_skip_keeps_the_last_level)
{
    PassPlan plan = makePlan(5);
    PassCursor cursor = {0, 0, 0};
    passAdvance(plan, cursor);
    for (int i = 0; i < 10; i++) {
        passSkipLevel(plan, cursor);
    }
    zassert_true(passAdvance(plan, cursor));
    zassert_equal(cursor.level, 5);
    passSkipLevel(plan, cursor);
    zassert_equal(cursor.skipLevels, 0);
    zassert_false(passAdvance(plan, cursor));
}

ZTEST(mode_pass_plan, test_skip_applies_after_the_last_start)
{
    PassPlan plan = makePlan(4, 0, 2);
    PassCursor cursor = {0, 0, 0};
    passAdvance(plan, cursor);
    passSkipLevel(plan, cursor);
    zassert_equal(passNextLevel(plan, cursor), 1);
    zassert_true(passAdvance(plan, cursor));
    zassert_equal(cursor.level, 1);
    zassert_equal(passNextLevel(plan, cursor), 3);
    zassert_true(passAdvance(plan, cursor));
    zassert_equal(cursor.level, 3);
    zassert_equal(cursor.startIndex, 0);
}
