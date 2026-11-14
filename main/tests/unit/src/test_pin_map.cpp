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

    Assignment &place(Fn fn, std::initializer_list<int32_t> gpios)
    {
        Assignment a;
        zassert_true(fromGpios(g_board, fn, gpios.begin(), gpios.size(), a));
        zassert_true(add(g_map, a));
        return g_map.list[g_map.count - 1];
    }

    Assignment &at(Fn fn)
    {
        for (size_t i = 0; i < g_map.count; i++) {
            if (g_map.list[i].fn == fn) {
                return g_map.list[i];
            }
        }
        zassert_unreachable("no %s", def(fn).id);
        return g_map.list[0];
    }

    void remove(Fn fn)
    {
        Map kept = {};
        memcpy(kept.names, g_map.names, sizeof(kept.names));
        for (size_t i = 0; i < g_map.count; i++) {
            if (g_map.list[i].fn != fn) {
                add(kept, g_map.list[i]);
            }
        }
        g_map = kept;
    }

    size_t g_warnings;
    void countWarning(const char *)
    {
        g_warnings++;
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
        zassert_equal(a.count, b.count);
        for (size_t i = 0; i < a.count; i++) {
            const Assignment &x = a.list[i];
            const Assignment &y = b.list[i];
            const char *id = def(x.fn).id;
            zassert_equal(x.fn, y.fn, "%s", id);
            zassert_equal(x.connector, y.connector, "%s", id);
            zassert_equal(x.start, y.start, "%s", id);
            zassert_equal(x.encoder, y.encoder, "%s", id);
            for (size_t s = 0; s < def(x.fn).signalCount; s++) {
                zassert_equal(x.options[s].invert, y.options[s].invert, "%s", id);
                zassert_equal(x.options[s].pull, y.options[s].pull, "%s", id);
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
    zassert_equal(gpioOf(g_board, at(Fn::ZMotor), 2), 7);
    zassert_equal(gpioOf(g_board, at(Fn::ZHandwheel), 0), 21);
    zassert_equal(at(Fn::Probe).options[0].pull, Pull::Up);
    zassert_equal(at(Fn::ZMotor).options[0].pull, Pull::None);
}

ZTEST(pin_map, test_ids_fit_storage)
{
    for (const FnDef &d : FUNCTIONS) {
        zassert_true(strlen(d.id) <= ID_MAX, "%s", d.id);
        zassert_true(!d.required || d.exclusive, "%s", d.id);
        zassert_true(!d.action || (d.dir == Dir::In && d.signalCount == 1 && !d.exclusive), "%s", d.id);
    }
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
    at(Fn::ZHandwheel).encoder = Encoder::Sw;
    zassert_equal(pcntUnit(g_map, Fn::ZHandwheel), -1);
    zassert_equal(pcntUnit(g_map, Fn::XHandwheel), 1);
    zassert_equal(pcntUnit(g_map, Fn::ZMotor), -1);
}

ZTEST(pin_map, test_rejects_missing_required)
{
    remove(Fn::ZMotor);
    zassert_not_null(check());
    setUp(nullptr);
    remove(Fn::XMotor);
    zassert_is_null(check(), "%s", g_err);
}

ZTEST(pin_map, test_exclusive_and_multiple)
{
    place(Fn::ZMotor, {38, 6, 5});
    zassert_not_null(check());
    zassert_not_null(strstr(g_err, "twice"), "%s", g_err);

    setUp(nullptr);
    place(Fn::Probe, {18});
    zassert_is_null(check(), "%s", g_err);
    zassert_equal(countOf(g_map, Fn::Probe), 2);
    zassert_equal(gpioOf(g_board, *find(g_map, Fn::Probe, 1), 0), 18);
    zassert_is_null(find(g_map, Fn::Probe, 2));
}

ZTEST(pin_map, test_rejects_conflict_reserved_power)
{
    Assignment &probe = at(Fn::Probe);
    probe = at(Fn::Spindle);
    probe.fn = Fn::Probe;
    probe.encoder = Encoder::None;
    zassert_not_null(check());

    setUp(nullptr);
    at(Fn::Probe).connector = 0;
    at(Fn::Probe).start = 3;
    zassert_not_null(check());

    setUp(nullptr);
    at(Fn::Probe).start = 0;
    zassert_not_null(check());

    setUp(nullptr);
    at(Fn::XMotor).connector = 3;
    at(Fn::XMotor).start = 2;
    zassert_not_null(check());
}

ZTEST(pin_map, test_rejects_bad_encoder_and_pull)
{
    at(Fn::Spindle).encoder = Encoder::Sw;
    zassert_not_null(check());

    setUp(nullptr);
    at(Fn::ZMotor).options[0].pull = Pull::Up;
    zassert_not_null(check());

    setUp(nullptr);
    g_board.pcntUnits = 2;
    zassert_not_null(check());
}

ZTEST(pin_map, test_keep_valid_first_wins)
{
    place(Fn::Probe, {21});
    place(Fn::ZMotor, {38, 6, 5});
    place(Fn::Probe, {18});
    Map out;
    g_warnings = 0;
    keepValid(g_board, g_map, out, countWarning);
    zassert_equal(g_warnings, 2);
    zassert_equal(countOf(out, Fn::Probe), 2);
    zassert_equal(gpioOf(g_board, *find(out, Fn::ZHandwheel), 0), 21);
    zassert_equal(gpioOf(g_board, *find(out, Fn::ZMotor), 0), 16);
    zassert_is_null(validate(g_board, out, g_err, sizeof(g_err)), "%s", g_err);
}

ZTEST(pin_map, test_stored_round_trip)
{
    strcpy(g_map.names[1], "Coolant");
    at(Fn::Probe).options[0] = {true, Pull::Down};
    place(Fn::Probe, {18});
    static Stored stored;
    toStored(g_board, g_map, stored);
    Map out;
    g_warnings = 0;
    zassert_true(fromStored(g_board, stored, out, countWarning));
    zassert_equal(g_warnings, 0);
    expectSameFunctions(out, g_map);
    zassert_str_equal(out.names[1], "Coolant");

    // A board with the connectors in another order still finds them by id.
    Board other = g_board;
    other.connectors[0] = g_board.connectors[1];
    other.connectors[1] = g_board.connectors[0];
    zassert_true(fromStored(other, stored, out, countWarning));
    zassert_equal(gpioOf(other, *find(out, Fn::ZMotor), 0), 16);
    zassert_str_equal(out.names[0], "Coolant");
}

ZTEST(pin_map, test_stored_skips_unknown)
{
    static Stored stored;
    toStored(g_board, g_map, stored);
    strcpy(stored.assignments[0].fn, "laser");
    strcpy(stored.assignments[1].connector, "r9c9");
    Map out;
    g_warnings = 0;
    zassert_true(fromStored(g_board, stored, out, countWarning));
    zassert_equal(g_warnings, 2);
    zassert_equal(out.count, g_map.count - 2);

    stored.magic = 0;
    zassert_false(fromStored(g_board, stored, out, countWarning));
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
    at(Fn::ZHandwheel).encoder = Encoder::Sw;
    at(Fn::Probe).options[0] = {true, Pull::Down};
    place(Fn::Probe, {18});
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

ZTEST(pin_map, test_functions_json)
{
    int len = PinMapJson::encodeFunctions(g_json, sizeof(g_json));
    zassert_true(len > 0);
    zassert_not_null(strstr(g_json, R"("id":"zHandwheel")"), "%s", g_json);
    zassert_not_null(strstr(g_json, R"("encoders":["hw","sw"])"), "%s", g_json);
    zassert_not_null(strstr(g_json, R"("signals":["ENA","DIR","STEP"])"), "%s", g_json);
    zassert_not_null(strstr(g_json, R"("id":"stop","label":"Stop","group":"Inputs","signals":["SIG"],"dir":"in",)"
                                    R"("required":false,"exclusive":false,"activeLow":true)"),
                     "%s", g_json);
}
