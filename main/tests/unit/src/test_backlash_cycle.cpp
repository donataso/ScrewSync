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
#include "BacklashCycle.hpp"

using namespace BacklashCycle;

ZTEST_SUITE(backlash_cycle, NULL, NULL, NULL, NULL, NULL);

namespace
{
    // probeDst 10000, backOff 1000, margin 500.
    Params basicParams(int dir, int runs)
    {
        return {dir, 10000, 1000, 500, runs};
    }

    Input moving(int64_t pos, int64_t motor, bool contact)
    {
        return {pos, motor, contact, false, 0, 0};
    }

    Input latch(int64_t pos, int64_t motor, bool contact)
    {
        return {pos, motor, contact, true, pos, motor};
    }

    // One run from Approach at pos to the next Approach (or Done): contact at
    // pos + dir*travel, release after slack motor steps, carriage stays put.
    Result run(State &s, const Params &p, int64_t pos, int64_t travel, int64_t slack)
    {
        int64_t contact = pos + p.dir * travel;
        tick(s, p, latch(contact, contact, true));
        zassert_equal((int)s.phase, (int)Phase::Release);
        int64_t releaseMotor = contact - p.dir * slack;
        tick(s, p, latch(contact, releaseMotor, false));
        zassert_equal((int)s.phase, (int)Phase::BackOff);
        return tick(s, p, moving(contact - p.dir * p.backOffSteps, releaseMotor - p.dir * p.backOffSteps, false));
    }
}

ZTEST(backlash_cycle, test_refuses_contact_at_start)
{
    State s;
    zassert_str_equal(begin(s, basicParams(-1, 3), 0, true), "contact present");
}

ZTEST(backlash_cycle, test_refuses_bad_run_count)
{
    State s;
    zassert_not_null(begin(s, basicParams(-1, 0), 0, false));
    zassert_not_null(begin(s, basicParams(-1, MAX_RUNS + 1), 0, false));
}

ZTEST(backlash_cycle, test_first_approach_at_probe_feed)
{
    State s;
    Params p = basicParams(-1, 3);
    zassert_is_null(begin(s, p, 0, false));
    zassert_equal((int)watching(s), (int)Watch::Contact);
    Result r = tick(s, p, moving(0, 0, false));
    zassert_true(r.move);
    zassert_false(r.reprobeFeed);
    zassert_equal(r.target, -10000);
}

ZTEST(backlash_cycle, test_release_moves_back_slowly)
{
    State s;
    Params p = basicParams(-1, 3);
    zassert_is_null(begin(s, p, 0, false));
    tick(s, p, latch(-5000, -5000, true));
    zassert_equal((int)watching(s), (int)Watch::Release);
    Result r = tick(s, p, moving(-5000, -4990, true));
    zassert_true(r.move);
    zassert_true(r.reprobeFeed);
    zassert_equal(r.target, -4000);
}

ZTEST(backlash_cycle, test_median_of_runs)
{
    State s;
    Params p = basicParams(-1, 3);
    zassert_is_null(begin(s, p, 0, false));
    Result r = run(s, p, 0, 5000, 40);
    zassert_false(r.done);
    zassert_equal((int)s.phase, (int)Phase::Approach);

    r = tick(s, p, moving(-4000, -4000, false));
    zassert_true(r.reprobeFeed);
    zassert_equal(r.target, -5500);

    r = run(s, p, -4000, 1000, 60);
    zassert_false(r.done);
    r = run(s, p, -4000, 1000, 50);
    zassert_true(r.done);
    zassert_equal(r.backlashSteps, 50);
}

ZTEST(backlash_cycle, test_positive_direction)
{
    State s;
    Params p = basicParams(1, 1);
    zassert_is_null(begin(s, p, 0, false));
    Result r = run(s, p, 0, 5000, 30);
    zassert_true(r.done);
    zassert_equal(r.backlashSteps, 30);
}

ZTEST(backlash_cycle, test_no_release_errors)
{
    State s;
    Params p = basicParams(-1, 1);
    zassert_is_null(begin(s, p, 0, false));
    tick(s, p, latch(-5000, -5000, true));
    Result r = tick(s, p, moving(-4000, -4000, true));
    zassert_str_equal(r.error, "no release");
}

ZTEST(backlash_cycle, test_glitches_error)
{
    State s;
    Params p = basicParams(-1, 1);
    zassert_is_null(begin(s, p, 0, false));
    zassert_str_equal(tick(s, p, latch(-5000, -5000, false)).error, "contact glitch");

    zassert_is_null(begin(s, p, 0, false));
    tick(s, p, latch(-5000, -5000, true));
    zassert_str_equal(tick(s, p, latch(-5000, -4950, true)).error, "release glitch");
}

ZTEST(backlash_cycle, test_contact_after_back_off_errors)
{
    State s;
    Params p = basicParams(-1, 1);
    zassert_is_null(begin(s, p, 0, false));
    tick(s, p, latch(-5000, -5000, true));
    tick(s, p, latch(-5000, -4950, false));
    zassert_str_equal(tick(s, p, moving(-4000, -3950, true)).error, "contact not lost");
}

ZTEST(backlash_cycle, test_even_median_averages)
{
    int64_t samples[] = {40, 10, 30, 20};
    zassert_equal(median(samples, 4), 25);
}
