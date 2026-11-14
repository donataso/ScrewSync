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
    constexpr size_t ID_MAX = 15;
    constexpr size_t MAX_ASSIGNMENTS = 32;
    constexpr size_t ERROR_MAX = 96;
    // PCNT glitch filter limit: 1023 APB cycles at 80 MHz.
    constexpr uint32_t MAX_HW_FILTER_NS = 12787;
    constexpr uint32_t MAX_SW_FILTER_NS = 5000000;

    struct Connector
    {
        const char *id;
        const char *title;
        uint8_t row;
        uint8_t col;
        uint8_t pinCount;
        int16_t pins[MAX_PINS];
    };

    enum class Fn : uint8_t
    {
        ZMotor,
        XMotor,
        Spindle,
        ZHandwheel,
        XHandwheel,
        Probe,
        StartHere,
        Stop,
        SetZero,
        SetZeroZ,
        SetZeroX,
        GoToZero,
        GoToZeroZ,
        GoToZeroX,
        FeedReset,
        FeedUp,
        FeedDown,
        ModeGear,
        ModeAsync,
        ModeTurn,
        ModeThread,
        ModePart,
        ModeSlot,
        ModeEllipse,
        ModeGcode,
        ModeProbe,
        Running,
        Count,
    };
    constexpr size_t FN_COUNT = (size_t)Fn::Count;

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
        // sw debounce is rounded up to this.
        uint32_t tickNs;
        // Encoder PPR from the settings, by function. 0 = not checked.
        float ppr[FN_COUNT];
    };

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
        const char *group;
        uint8_t signalCount;
        const char *signals[MAX_SIGNALS];
        Dir dir;
        bool required;
        // At most one assignment.
        bool exclusive;
        bool hwEncoder;
        bool swEncoder;
        // Default invert of new assignments.
        bool activeLow;
        // Switch: commands run on press, separated by ';'. nullptr otherwise.
        const char *action;
        // Default encoder filter: PCNT glitch filter (hw) or debounce time (sw).
        uint32_t filterNs;
        // Encoder top speed for the edge rate check. 0 = not checked.
        uint32_t maxRpm;
    };

    extern const FnDef FUNCTIONS[FN_COUNT];

    struct SignalOptions
    {
        bool invert;
        Pull pull;
    };

    struct Assignment
    {
        Fn fn;
        uint8_t connector;
        uint8_t start;
        Encoder encoder;
        SignalOptions options[MAX_SIGNALS];
        uint32_t filterNs;
    };

    struct Map
    {
        Assignment list[MAX_ASSIGNMENTS];
        size_t count;
        // Empty = the connector's title.
        char names[MAX_CONNECTORS][CONNECTOR_NAME_MAX + 1];
    };

    const FnDef &def(Fn fn);
    int findFn(const char *id);
    int findConnector(const Board &board, const char *id);

    // nth assignment of fn, or nullptr.
    const Assignment *find(const Map &map, Fn fn, size_t nth = 0);
    size_t countOf(const Map &map, Fn fn);
    bool add(Map &map, const Assignment &a);

    // GPIO of one signal, or -1.
    int gpioOf(const Board &board, const Assignment &a, size_t signal);

    // PCNT unit of a hardware encoder, or -1. The spindle always has unit 0.
    int pcntUnit(const Map &map, Fn fn);

    bool isReserved(const Board &board, int gpio);

    // Places fn on the connector whose pins match gpios, with default options.
    bool fromGpios(const Board &board, Fn fn, const int32_t *gpios, size_t count, Assignment &out);

    // nullptr if a fits the board and does not clash with the kept assignments.
    const char *checkAssignment(const Board &board, const Assignment *kept, size_t keptCount, const Assignment &a,
                                char *err, size_t errMax);
    const char *missingRequired(const Map &map, char *err, size_t errMax);
    // nullptr if the encoder filter lets through the edges of the function's top speed.
    const char *checkEdgeRate(const Board &board, const Assignment &a, char *err, size_t errMax);

    // nullptr if the map is valid, else the first problem.
    const char *validate(const Board &board, const Map &map, char *err, size_t errMax);

    // Copies the valid assignments and names of in to out. In a clash the first one stays.
    using Warn = void (*)(const char *msg);
    void keepValid(const Board &board, const Map &in, Map &out, Warn warn);

    // Saved form, keyed by id, so new functions or connectors do not break a saved map.
    constexpr uint32_t STORED_MAGIC = 0x50494e33; // "PIN3"

    struct StoredAssignment
    {
        char fn[ID_MAX + 1];
        char connector[ID_MAX + 1];
        uint8_t start;
        uint8_t encoder;
        SignalOptions options[MAX_SIGNALS];
        uint32_t filterNs;
    };

    struct StoredName
    {
        char connector[ID_MAX + 1];
        char name[CONNECTOR_NAME_MAX + 1];
    };

    struct Stored
    {
        uint32_t magic;
        uint32_t assignmentCount;
        uint32_t nameCount;
        StoredAssignment assignments[MAX_ASSIGNMENTS];
        StoredName names[MAX_CONNECTORS];
    };

    void toStored(const Board &board, const Map &map, Stored &out);
    // False if in is not a stored map. Skips (with warn) what this firmware or board does not know.
    bool fromStored(const Board &board, const Stored &in, Map &out, Warn warn);
    const char *validateName(const char *name);
}

#endif
