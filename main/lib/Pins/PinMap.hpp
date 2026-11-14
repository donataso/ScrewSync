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
#ifndef PinMap_h
#define PinMap_h

#include <stddef.h>
#include <stdint.h>

// Board connectors and the user's assignment of functions to their pins.
// Pure logic, no Zephyr calls.
namespace PinMap
{
    constexpr int16_t POWER_5V = -1;
    constexpr int16_t POWER_GND = -2;

    constexpr size_t MAX_CONNECTORS = 32;
    constexpr size_t MAX_PINS = 16;
    constexpr size_t MAX_SIGNALS = 3;
    constexpr size_t MAX_RESERVED = 16;
    constexpr size_t MAX_STRAPPING = 8;
    constexpr size_t CONNECTOR_NAME_MAX = 20;
    constexpr size_t ERROR_MAX = 96;

    struct Connector
    {
        const char *id;
        const char *title;
        uint8_t row;
        uint8_t col;
        uint8_t pinCount;
        int16_t pins[MAX_PINS];
    };

    struct Board
    {
        const char *name;
        Connector connectors[MAX_CONNECTORS];
        size_t connectorCount;
        uint8_t reserved[MAX_RESERVED];
        const char *reservedNotes[MAX_RESERVED];
        size_t reservedCount;
        uint8_t strapping[MAX_STRAPPING];
        size_t strappingCount;
        uint8_t pcntUnits;
    };

    enum class Fn : uint8_t
    {
        ZMotor,
        XMotor,
        Spindle,
        ZHandwheel,
        XHandwheel,
        Probe,
        Count,
    };
    constexpr size_t FN_COUNT = (size_t)Fn::Count;

    enum class Dir : uint8_t
    {
        In,
        Out,
    };

    enum class Encoder : uint8_t
    {
        None,
        Hw,
        Sw,
    };

    enum class Pull : uint8_t
    {
        None,
        Up,
        Down,
    };

    struct FnDef
    {
        const char *id;
        const char *label;
        uint8_t signalCount;
        const char *signals[MAX_SIGNALS];
        Dir dir;
        bool required;
        bool hwEncoder;
        bool swEncoder;
    };

    extern const FnDef FUNCTIONS[FN_COUNT];

    struct SignalOptions
    {
        bool invert;
        Pull pull;
    };

    struct Assignment
    {
        bool used;
        uint8_t connector;
        uint8_t start;
        Encoder encoder;
        SignalOptions options[MAX_SIGNALS];
    };

    struct Map
    {
        Assignment fn[FN_COUNT];
        // Empty = the connector's title.
        char names[MAX_CONNECTORS][CONNECTOR_NAME_MAX + 1];
    };

    const FnDef &def(Fn fn);
    int findFn(const char *id);
    int findConnector(const Board &board, const char *id);

    // GPIO of one signal, or -1.
    int gpioOf(const Board &board, const Map &map, Fn fn, size_t signal);

    // PCNT unit of a hardware encoder, or -1. The spindle always has unit 0.
    int pcntUnit(const Map &map, Fn fn);

    bool isReserved(const Board &board, int gpio);

    // Places fn on the connector whose pins match gpios, with default options.
    bool fromGpios(const Board &board, Fn fn, const int32_t *gpios, size_t count, Assignment &out);

    // nullptr if the map is valid, else the first problem.
    const char *validate(const Board &board, const Map &map, char *err, size_t errMax);
    const char *validateName(const char *name);
}

#endif
