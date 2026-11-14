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
#include "JogStep.hpp"

using namespace Jog;

ZTEST_SUITE(jog_step, nullptr, nullptr, nullptr, nullptr, nullptr);

ZTEST(jog_step, test_cycle_presets_then_custom)
{
    const int32_t *p = stepPresets(false);
    zassert_equal(nextStep(p[0], 123, false), p[1]);
    zassert_equal(nextStep(p[1], 123, false), p[2]);
    zassert_equal(nextStep(p[2], 123, false), 123);
    zassert_equal(nextStep(123, 123, false), p[0]);
}

ZTEST(jog_step, test_cycle_skips_missing_custom)
{
    const int32_t *p = stepPresets(true);
    zassert_equal(nextStep(p[2], 0, true), p[0]);
    zassert_equal(nextStep(p[2], p[1], true), p[0]);
}

ZTEST(jog_step, test_cycle_from_other_units)
{
    zassert_equal(nextStep(stepPresets(false)[1], 0, true), stepPresets(true)[0]);
}

ZTEST(jog_step, test_dial_mm)
{
    zassert_equal(dialStep(1000, 1, 1, false), 1010);
    zassert_equal(dialStep(1000, -2, 10, false), 800);
    zassert_equal(dialStep(10, -5, 1, false), 10);
    zassert_equal(dialStep(MAX_STEP_DU, 3, 100, false), MAX_STEP_DU);
}

ZTEST(jog_step, test_dial_inch)
{
    zassert_equal(dialStep(254, 1, 1, true), 257);
    zassert_equal(dialStep(254, 1, 10, true), 279);
    zassert_equal(dialStep(3, -1, 1, true), 3);
}

ZTEST(jog_step, test_multiplier_by_speed)
{
    zassert_equal(dialMultiplier(500), 1);
    zassert_equal(dialMultiplier(50), 10);
    zassert_equal(dialMultiplier(10), 100);
    zassert_true(dialMultiplier(0) >= dialMultiplier(10));
}
