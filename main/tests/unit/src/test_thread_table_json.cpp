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
#include <string.h>
#include "ThreadStore.hpp"

using namespace ThreadTable;
using ThreadStore::fromJson;
using ThreadStore::JsonTable;
using ThreadStore::toJson;

namespace
{
    Table g_table;
    Table g_out;
    JsonTable g_json;

    void loadTable(void *)
    {
        loadDefaults(g_table);
        memset(&g_out, 0, sizeof(g_out));
        toJson(g_table, g_json);
    }

    void expectSameTable()
    {
        zassert_equal(g_out.groupCount, g_table.groupCount);
        zassert_equal(g_out.entryCount, g_table.entryCount);
        for (size_t i = 0; i < g_table.groupCount; i++) {
            zassert_str_equal(g_out.groups[i].name, g_table.groups[i].name);
            zassert_equal(g_out.groups[i].depthExternal, g_table.groups[i].depthExternal);
            zassert_equal(g_out.groups[i].depthInternal, g_table.groups[i].depthInternal);
            zassert_equal(g_out.groups[i].taper, g_table.groups[i].taper);
            zassert_equal(g_out.groups[i].flankAngle, g_table.groups[i].flankAngle);
        }
        for (size_t i = 0; i < g_table.entryCount; i++) {
            const Entry &a = g_out.entries[i];
            const Entry &b = g_table.entries[i];
            zassert_str_equal(a.designation, b.designation);
            zassert_equal(a.group, b.group);
            zassert_equal(a.majorDu, b.majorDu);
            zassert_equal(a.pitchDu, b.pitchDu);
            zassert_equal(a.depthExternalDu, b.depthExternalDu);
            zassert_equal(a.depthInternalDu, b.depthInternalDu);
            zassert_equal(a.hasTaper, b.hasTaper);
            zassert_equal(a.taper, b.taper);
        }
    }
}

ZTEST_SUITE(thread_table_json, NULL, NULL, loadTable, NULL, NULL);

ZTEST(thread_table_json, test_defaults_round_trip)
{
    zassert_equal(g_json.groupCount, g_table.groupCount);
    zassert_equal(g_json.entryCount, g_table.entryCount);
    zassert_equal(g_json.overrideCount, 0);
    zassert_is_null(fromJson(g_json, g_out));
    expectSameTable();
}

ZTEST(thread_table_json, test_entries_name_their_group)
{
    const Entry &last = g_table.entries[g_table.entryCount - 1];
    zassert_str_equal(g_json.entries[g_table.entryCount - 1].group, g_table.groups[last.group].name);
    zassert_str_equal(g_json.entries[g_table.entryCount - 1].designation, last.designation);
}

ZTEST(thread_table_json, test_overrides_round_trip)
{
    g_table.entries[0].depthExternalDu = 1234;
    g_table.entries[3].depthInternalDu = 2345;
    g_table.entries[5].hasTaper = true;
    g_table.entries[5].taper = 0.1f;
    toJson(g_table, g_json);
    zassert_equal(g_json.overrideCount, 3);
    zassert_str_equal(g_json.overrides[2].designation, g_table.entries[5].designation);
    zassert_is_null(fromJson(g_json, g_out));
    expectSameTable();
}

ZTEST(thread_table_json, test_taper_needs_has_taper)
{
    g_table.entries[0].depthExternalDu = 1234;
    toJson(g_table, g_json);
    g_json.overrides[0].taper = 0.2f;
    zassert_is_null(fromJson(g_json, g_out));
    zassert_false(g_out.entries[0].hasTaper);
    zassert_equal(g_out.entries[0].taper, 0);
}

ZTEST(thread_table_json, test_refuses_entry_of_unknown_group)
{
    g_json.entries[0].group = "nope";
    zassert_str_equal(fromJson(g_json, g_out), "entry without group");
    g_json.entries[0].group = nullptr;
    zassert_str_equal(fromJson(g_json, g_out), "entry without group");
}

ZTEST(thread_table_json, test_refuses_bad_names)
{
    g_json.groups[0].name = "a-group-name-too-long";
    zassert_str_equal(fromJson(g_json, g_out), "bad group name");
    toJson(g_table, g_json);
    g_json.groups[0].name = nullptr;
    zassert_str_equal(fromJson(g_json, g_out), "bad group name");
    toJson(g_table, g_json);
    g_json.entries[0].designation = "a-designation-too-long";
    zassert_str_equal(fromJson(g_json, g_out), "bad designation");
    toJson(g_table, g_json);
    g_json.entries[0].designation = nullptr;
    zassert_str_equal(fromJson(g_json, g_out), "bad designation");
}

ZTEST(thread_table_json, test_refuses_override_without_thread)
{
    g_json.overrideCount = 1;
    g_json.overrides[0] = {"metric", "M999", 100, 0, false, 0};
    zassert_str_equal(fromJson(g_json, g_out), "override without thread");
    g_json.overrides[0] = {nullptr, "M10", 100, 0, false, 0};
    zassert_str_equal(fromJson(g_json, g_out), "override without thread");
}

ZTEST(thread_table_json, test_result_is_validated)
{
    g_json.groups[0].flankAngle = 0;
    zassert_not_null(fromJson(g_json, g_out));
    toJson(g_table, g_json);
    g_json.entries[0].pitchDu = 0;
    zassert_not_null(fromJson(g_json, g_out));
    toJson(g_table, g_json);
    g_json.entries[1].designation = g_json.entries[0].designation;
    zassert_not_null(fromJson(g_json, g_out));
}
