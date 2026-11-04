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
#include <stdio.h>
#include "ThreadTable.hpp"

using namespace ThreadTable;

static Table g_table;

static void *setup(void)
{
    loadDefaults(g_table);
    return NULL;
}

ZTEST_SUITE(thread_table, NULL, setup, NULL, NULL, NULL);

ZTEST(thread_table, test_defaults_validate)
{
    zassert_is_null(validate(g_table));
}

ZTEST(thread_table, test_defaults_complete)
{
    static Table t;
    zassert_equal(loadDefaults(t), 0);
    size_t metric = 0;
    for (size_t i = 0; i < t.entryCount; i++) {
        metric += t.entries[i].group == findGroup(t, "metric");
    }
    zassert_equal(metric, MAX_GROUP_ENTRIES);
}

ZTEST(thread_table, test_validate_rejects_full_group)
{
    static Table t;
    loadDefaults(t);
    Entry extra = t.entries[0];
    snprintf(extra.designation, sizeof(extra.designation), "M99");
    t.entries[t.entryCount++] = extra;
    zassert_not_null(validate(t));
}

ZTEST(thread_table, test_metric_coarse_and_fine)
{
    const Entry *coarse = find(g_table, "metric", "M10");
    const Entry *fine = find(g_table, "metric", "M10x1.25");
    zassert_not_null(coarse);
    zassert_not_null(fine);
    zassert_equal(coarse->pitchDu, 15000);
    zassert_equal(coarse->majorDu, 100000);
    zassert_equal(fine->pitchDu, 12500);
    zassert_not_null(find(g_table, "metric", "M1.6"));
    zassert_not_null(find(g_table, "metric", "M24x2"));
}

ZTEST(thread_table, test_inch_pitch_from_tpi)
{
    // 25.4mm / 20 = 1.27mm
    const Entry *e = find(g_table, "unc", "1/4-20UNC");
    zassert_not_null(e);
    zassert_equal(e->pitchDu, 12700);
    zassert_not_null(find(g_table, "unc", "2-4.5UNC"));
    zassert_not_null(find(g_table, "bsp", "G1/2"));
}

ZTEST(thread_table, test_depth_and_taper)
{
    const Group &metric = g_table.groups[findGroup(g_table, "metric")];
    // 0.61343 * 1.5mm external, 0.54127 * 1.5mm internal
    zassert_equal(depthDu(metric, 15000, false), 9201);
    zassert_equal(depthDu(metric, -15000, true), 8119);
    const Group &npt = g_table.groups[findGroup(g_table, "npt")];
    zassert_within(npt.taper, 0.0625f, 1e-6f);
}

ZTEST(thread_table, test_validate_rejects_duplicates)
{
    static Table t;
    loadDefaults(t);
    t.entries[1] = t.entries[0];
    zassert_not_null(validate(t));
}

ZTEST(thread_table, test_validate_rejects_commas)
{
    static Table t;
    loadDefaults(t);
    snprintf(t.entries[0].designation, sizeof(t.entries[0].designation), "M6,1");
    zassert_not_null(validate(t));
    loadDefaults(t);
    snprintf(t.groups[0].name, sizeof(t.groups[0].name), "a,b");
    zassert_not_null(validate(t));
}

ZTEST(thread_table, test_overrides)
{
    static Table t;
    loadDefaults(t);
    Entry &e = t.entries[0];
    zassert_equal(depthDu(t, e, false), depthDu(t.groups[e.group], e.pitchDu, false));
    zassert_equal(taper(t, e), t.groups[e.group].taper);
    e.depthExternalDu = 1234;
    e.hasTaper = true;
    e.taper = 0.1f;
    zassert_equal(depthDu(t, e, false), 1234);
    zassert_equal(depthDu(t, e, true), depthDu(t.groups[e.group], e.pitchDu, true));
    zassert_within(taper(t, e), 0.1f, 1e-6f);
    zassert_is_null(validate(t));
}

ZTEST(thread_table, test_validate_rejects_bad_overrides)
{
    static Table t;
    loadDefaults(t);
    t.entries[0].depthInternalDu = 2 * t.entries[0].pitchDu;
    zassert_not_null(validate(t));
    loadDefaults(t);
    t.entries[0].hasTaper = true;
    t.entries[0].taper = 1;
    zassert_not_null(validate(t));
}
