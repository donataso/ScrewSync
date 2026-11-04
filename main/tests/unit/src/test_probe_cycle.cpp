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
#include "ProbeCycle.hpp"
#include "ProbeInput.hpp"

using namespace ProbeCycle;

ZTEST_SUITE(probe_cycle, NULL, NULL, NULL, NULL, NULL);

namespace
{
    // probeDst 10000, backOff 1000, margin 500.
    Params basicParams(int dir, int64_t offset)
    {
        return {dir, 10000, 1000, 500, offset};
    }

    // Starts at 0, first contact at dir*5000, backs off to dir*4000.
    void toReprobe(State &s, const Params &p)
    {
        zassert_is_null(begin(s, p, 0, false));
        int64_t first = p.dir * 5000;
        tick(s, p, first, true, true, first);
        zassert_equal((int)s.phase, (int)Phase::BackOff1);
        tick(s, p, first - p.dir * 1000, false, false, 0);
        zassert_equal((int)s.phase, (int)Phase::Reprobe);
    }
}

ZTEST(probe_cycle, test_refuses_contact_at_start)
{
    State s;
    const char *err = begin(s, basicParams(-1, 0), 0, true);
    zassert_not_null(err);
    zassert_str_equal(err, "contact present");
}

ZTEST(probe_cycle, test_approach_targets_probe_dst)
{
    State s;
    Params p = basicParams(-1, 0);
    zassert_is_null(begin(s, p, 0, false));
    zassert_true(watching(s));
    Result r = tick(s, p, 0, false, false, 0);
    zassert_true(r.move);
    zassert_false(r.reprobeFeed);
    zassert_equal(r.target, -10000);
}

ZTEST(probe_cycle, test_no_contact_within_probe_dst_errors)
{
    State s;
    Params p = basicParams(-1, 0);
    zassert_is_null(begin(s, p, 0, false));
    Result r = tick(s, p, -10000, false, false, 0);
    zassert_str_equal(r.error, "no contact");
}

ZTEST(probe_cycle, test_glitch_on_approach_errors)
{
    State s;
    Params p = basicParams(-1, 0);
    zassert_is_null(begin(s, p, 0, false));
    Result r = tick(s, p, -3000, false, true, -3000);
    zassert_str_equal(r.error, "contact glitch");
}

ZTEST(probe_cycle, test_back_off_is_fixed_distance)
{
    State s;
    Params p = basicParams(-1, 0);
    zassert_is_null(begin(s, p, 0, false));
    tick(s, p, -5000, true, true, -5000);
    Result r = tick(s, p, -5000, true, false, 0);
    zassert_true(r.move);
    zassert_true(r.reprobeFeed);
    zassert_equal(r.target, -4000);
}

ZTEST(probe_cycle, test_contact_after_back_off_errors)
{
    State s;
    Params p = basicParams(-1, 0);
    zassert_is_null(begin(s, p, 0, false));
    tick(s, p, -5000, true, true, -5000);
    Result r = tick(s, p, -4000, true, false, 0);
    zassert_str_equal(r.error, "contact not lost");
}

ZTEST(probe_cycle, test_reprobe_limit_is_back_off_plus_margin)
{
    State s;
    Params p = basicParams(-1, 0);
    toReprobe(s, p);
    zassert_true(watching(s));
    Result r = tick(s, p, -4000, false, false, 0);
    zassert_true(r.reprobeFeed);
    zassert_equal(r.target, -5500);
    r = tick(s, p, -5500, false, false, 0);
    zassert_str_equal(r.error, "no contact");
}

ZTEST(probe_cycle, test_glitch_on_reprobe_errors)
{
    State s;
    Params p = basicParams(-1, 0);
    toReprobe(s, p);
    Result r = tick(s, p, -4900, false, true, -4900);
    zassert_str_equal(r.error, "contact glitch");
}

ZTEST(probe_cycle, test_contact_after_final_back_off_errors)
{
    State s;
    Params p = basicParams(-1, 0);
    toReprobe(s, p);
    tick(s, p, -5010, true, true, -5010);
    zassert_equal((int)s.phase, (int)Phase::BackOff2);
    Result r = tick(s, p, -4010, true, false, 0);
    zassert_str_equal(r.error, "contact not lost");
}

ZTEST(probe_cycle, test_zero_surface_all_directions)
{
    struct Case
    {
        int dir;
        int64_t offset;
    };
    const Case cases[] = {{-1, 300}, {1, 300}, {-1, 0}, {1, 0}};
    for (const Case &c : cases)
    {
        State s;
        Params p = basicParams(c.dir, c.offset);
        toReprobe(s, p);
        int64_t second = c.dir * 5010;
        tick(s, p, second, true, true, second);
        Result r = tick(s, p, second - c.dir * 1000, false, false, 0);
        zassert_true(r.done);
        zassert_equal(r.surfaceSteps, second + c.dir * c.offset);
    }
}

ZTEST_SUITE(probe_input_polarity, NULL, NULL, NULL, NULL, NULL);

ZTEST(probe_input_polarity, test_no_polarity_contact_is_inverted)
{
    // Normally-open: idle (no contact) reads high (pulled up); contact
    // grounds the pin.
    zassert_false(ProbeInput::fromRaw(true, false));
    zassert_true(ProbeInput::fromRaw(false, false));
}

ZTEST(probe_input_polarity, test_nc_polarity_contact_matches_raw)
{
    // Normally-closed: idle reads low (grounded); contact opens the
    // circuit, so the pull-up reads high.
    zassert_false(ProbeInput::fromRaw(false, true));
    zassert_true(ProbeInput::fromRaw(true, true));
}
