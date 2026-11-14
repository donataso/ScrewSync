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
#include <initializer_list>
#include <string.h>
#include "PinMap.hpp"
#include "PinMapJson.hpp"

using namespace PinMap;

namespace
{
    constexpr int16_t V = POWER_5V;
    constexpr int16_t G = POWER_GND;

    Board g_board;
    Map g_map;
    char g_err[ERROR_MAX];
    char g_json[4096];

    void addConnector(const char *id, const char *title, uint8_t row, uint8_t col, std::initializer_list<int16_t> pins)
    {
        Connector &c = g_board.connectors[g_board.connectorCount++];
        c = {id, title, row, col, (uint8_t)pins.size(), {}};
        size_t i = 0;
        for (int16_t p : pins) {
            c.pins[i++] = p;
        }
    }

    void place(Fn fn, std::initializer_list<int32_t> gpios)
    {
        zassert_true(fromGpios(g_board, fn, gpios.begin(), gpios.size(), g_map.fn[(size_t)fn]));
    }

    void setUp(void *)
    {
        g_board = {};
        g_board.name = "test";
        addConnector("r1c1", "Spare 1", 1, 1, {V, 41, 42, 35});
        addConnector("r1c2", "Z motor", 1, 2, {V, 16, 15, 7});
        addConnector("r1c3", "X motor", 1, 3, {V, 1, 2, 17});
        addConnector("r1c4", "Spare 2", 1, 4, {38, 6, 5, 4});
        addConnector("r2c1", "PROBE", 2, 1, {V, G, 8, 18});
        addConnector("r2c2", "Z handwheel", 2, 2, {V, G, 21, 47});
        addConnector("r2c3", "X handwheel", 2, 3, {V, G, 3, 46});
        addConnector("r3c5", "ENCODER", 3, 5, {V, G, 10, 11});
        g_board.reserved[0] = 35;
        g_board.reservedNotes[0] = "PSRAM";
        g_board.reservedCount = 1;
        g_board.strapping[0] = 3;
        g_board.strappingCount = 1;
        g_board.pcntUnits = 4;

        memset(&g_map, 0, sizeof(g_map));
        place(Fn::ZMotor, {16, 15, 7});
        place(Fn::XMotor, {1, 2, 17});
        place(Fn::Spindle, {10, 11});
        place(Fn::ZHandwheel, {21, 47});
        place(Fn::XHandwheel, {3, 46});
        place(Fn::Probe, {8});
    }

    void expectSameFunctions(const Map &a, const Map &b)
    {
        for (size_t i = 0; i < FN_COUNT; i++) {
            const Assignment &x = a.fn[i];
            const Assignment &y = b.fn[i];
            zassert_equal(x.used, y.used, "%s", FUNCTIONS[i].id);
            if (!x.used) {
                continue;
            }
            zassert_equal(x.connector, y.connector, "%s", FUNCTIONS[i].id);
            zassert_equal(x.start, y.start, "%s", FUNCTIONS[i].id);
            zassert_equal(x.encoder, y.encoder, "%s", FUNCTIONS[i].id);
            for (size_t s = 0; s < FUNCTIONS[i].signalCount; s++) {
                zassert_equal(x.options[s].invert, y.options[s].invert, "%s", FUNCTIONS[i].id);
                zassert_equal(x.options[s].pull, y.options[s].pull, "%s", FUNCTIONS[i].id);
            }
        }
    }

    const char *check()
    {
        return validate(g_board, g_map, g_err, sizeof(g_err));
    }
}

ZTEST_SUITE(pin_map, nullptr, nullptr, setUp, nullptr, nullptr);

ZTEST(pin_map, test_defaults_are_valid)
{
    zassert_is_null(check(), "%s", g_err);
    zassert_equal(gpioOf(g_board, g_map, Fn::ZMotor, 2), 7);
    zassert_equal(gpioOf(g_board, g_map, Fn::ZHandwheel, 0), 21);
    zassert_equal(g_map.fn[(size_t)Fn::Probe].options[0].pull, Pull::Up);
    zassert_equal(g_map.fn[(size_t)Fn::ZMotor].options[0].pull, Pull::None);
}

ZTEST(pin_map, test_from_gpios_needs_adjacent_pins)
{
    Assignment a;
    const int32_t split[] = {16, 7, 15};
    zassert_false(fromGpios(g_board, Fn::ZMotor, split, 3, a));
    const int32_t shortList[] = {16, 15};
    zassert_false(fromGpios(g_board, Fn::ZMotor, shortList, 2, a));
}

ZTEST(pin_map, test_pcnt_units)
{
    zassert_equal(pcntUnit(g_map, Fn::Spindle), 0);
    zassert_equal(pcntUnit(g_map, Fn::ZHandwheel), 1);
    zassert_equal(pcntUnit(g_map, Fn::XHandwheel), 2);
    g_map.fn[(size_t)Fn::ZHandwheel].encoder = Encoder::Sw;
    zassert_equal(pcntUnit(g_map, Fn::ZHandwheel), -1);
    zassert_equal(pcntUnit(g_map, Fn::XHandwheel), 1);
    zassert_equal(pcntUnit(g_map, Fn::ZMotor), -1);
}

ZTEST(pin_map, test_rejects_missing_required)
{
    g_map.fn[(size_t)Fn::ZMotor].used = false;
    zassert_not_null(check());
    setUp(nullptr);
    g_map.fn[(size_t)Fn::XMotor].used = false;
    zassert_is_null(check(), "%s", g_err);
}

ZTEST(pin_map, test_rejects_conflict_reserved_power)
{
    g_map.fn[(size_t)Fn::Probe] = g_map.fn[(size_t)Fn::Spindle];
    g_map.fn[(size_t)Fn::Probe].encoder = Encoder::None;
    zassert_not_null(check());

    setUp(nullptr);
    g_map.fn[(size_t)Fn::Probe].connector = 0;
    g_map.fn[(size_t)Fn::Probe].start = 3;
    zassert_not_null(check());

    setUp(nullptr);
    g_map.fn[(size_t)Fn::Probe].start = 0;
    zassert_not_null(check());

    setUp(nullptr);
    g_map.fn[(size_t)Fn::XMotor].connector = 3;
    g_map.fn[(size_t)Fn::XMotor].start = 2;
    zassert_not_null(check());
}

ZTEST(pin_map, test_rejects_bad_encoder_and_pull)
{
    g_map.fn[(size_t)Fn::Spindle].encoder = Encoder::Sw;
    zassert_not_null(check());

    setUp(nullptr);
    g_map.fn[(size_t)Fn::ZMotor].options[0].pull = Pull::Up;
    zassert_not_null(check());

    setUp(nullptr);
    g_board.pcntUnits = 2;
    zassert_not_null(check());
}

ZTEST(pin_map, test_names)
{
    strcpy(g_map.names[0], "Coolant");
    strcpy(g_map.names[3], "Coolant");
    zassert_is_null(check(), "%s", g_err);
    strcpy(g_map.names[1], " Pad");
    zassert_not_null(check());
    strcpy(g_map.names[1], "");
    strcpy(g_map.names[20], "x");
    zassert_not_null(check());
    zassert_not_null(validateName("a\tb"));
    zassert_is_null(validateName("Ok name"));
}

ZTEST(pin_map, test_json_round_trip)
{
    strcpy(g_map.names[0], "Coolant");
    g_map.fn[(size_t)Fn::ZHandwheel].encoder = Encoder::Sw;
    g_map.fn[(size_t)Fn::Probe].options[0] = {true, Pull::Down};
    int len = PinMapJson::encode(g_board, g_map, g_json, sizeof(g_json));
    zassert_true(len > 0);
    zassert_not_null(strstr(g_json, "\"pins\":[-1,41,42,35]"), "%s", g_json);

    Map out;
    zassert_is_null(PinMapJson::decode(g_board, g_json, (size_t)len, out, g_err, sizeof(g_err)), "%s", g_err);
    expectSameFunctions(out, g_map);
    zassert_str_equal(out.names[0], "Coolant");
}

ZTEST(pin_map, test_json_rejects)
{
    Map out;
    char unknown[] = R"({"functions":[{"id":"nope","connector":"r1c2","start":1,"encoder":"","signals":[]}]})";
    zassert_not_null(PinMapJson::decode(g_board, unknown, strlen(unknown), out, g_err, sizeof(g_err)));

    char noZ[] = R"({"functions":[{"id":"spindle","connector":"r3c5","start":2,"encoder":"hw",)"
                 R"("signals":[{"invert":false,"pull":"up"},{"invert":false,"pull":"up"}]}]})";
    zassert_not_null(PinMapJson::decode(g_board, noZ, strlen(noZ), out, g_err, sizeof(g_err)));
    zassert_not_null(strstr(g_err, "Z motor"), "%s", g_err);

    char badPull[] = R"({"functions":[{"id":"probe","connector":"r2c1","start":2,"encoder":"",)"
                     R"("signals":[{"invert":false,"pull":"sideways"}]}]})";
    zassert_not_null(PinMapJson::decode(g_board, badPull, strlen(badPull), out, g_err, sizeof(g_err)));
}
