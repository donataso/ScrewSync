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
#include "ModeCells.hpp"
#include <zephyr/sys/util.h>

namespace
{
    using ModeCells::Cell;
    using ModeCells::Kind;

    constexpr Cell value(Kind kind, const char *param, const char *label, const char *when = nullptr)
    {
        return {kind, param, label, nullptr, nullptr, nullptr, nullptr, when};
    }

    constexpr Cell toggle(const char *param, const char *labelA, const char *valueA, const char *labelB,
                          const char *valueB)
    {
        return {Kind::Toggle, param, nullptr, labelA, valueA, labelB, valueB, nullptr};
    }

    constexpr Cell picker(const char *label)
    {
        return {Kind::Picker, nullptr, label, nullptr, nullptr, nullptr, nullptr, nullptr};
    }

    constexpr Cell latch(const char *param, const char *label)
    {
        return {Kind::Latch, param, label, nullptr, nullptr, nullptr, nullptr, nullptr};
    }

    constexpr Cell GAP = {Kind::Gap, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr};

    constexpr Cell AXIS = toggle("axis", "Z", "z", "X", "x");
    constexpr Cell DIR = toggle("dir", nullptr, "-", nullptr, "+");
    constexpr Cell SIDE = toggle("internal", "Ext", "off", "Int", "on");
    constexpr Cell SPINDLE = toggle("spindle", "Fwd", "fwd", "Rev", "rev");

    constexpr Cell GEAR[] = {
        value(Kind::Pitch, "pitch", "Feed"),
        AXIS,
        value(Kind::Length, "stop-after", "Stop"),
        DIR,
        value(Kind::Angle, "taper", "Taper", "axis=z"),
    };

    constexpr Cell ASYNC[] = {
        value(Kind::Speed, "speed", "Speed"),
        AXIS,
        value(Kind::Length, "stop-after", "Stop"),
        DIR,
    };

    constexpr Cell TURN[] = {
        value(Kind::Feed, "pitch", "Feed"),
        AXIS,
        value(Kind::Length, "depth", "Depth"),
        DIR,
        value(Kind::Length, "length", "Length"),
        SIDE,
        value(Kind::Count, "passes", "Passes"),
        SPINDLE,
    };

    constexpr Cell THREAD[] = {
        value(Kind::Pitch, "pitch", "Pitch"),
        toggle("hand", "LH", "lh", "RH", "rh"),
        value(Kind::Length, "depth", "Depth"),
        SIDE,
        value(Kind::Length, "length", "Length"),
        SPINDLE,
        GAP,
        GAP,
        GAP,
        GAP,
        value(Kind::Count, "passes", "Passes"),
        value(Kind::Count, "starts", "Starts"),
        value(Kind::Angle, "taper", "Taper"),
        latch("flank", "Flank"),
        latch("spring", "Spring"),
        picker("Library"),
    };

    constexpr Cell PART[] = {
        value(Kind::Feed, "pitch", "Feed"),
        SIDE,
        value(Kind::Length, "depth", "Depth"),
        SPINDLE,
        value(Kind::Length, "peck", "Peck"),
    };

    constexpr Cell SLOT[] = {
        value(Kind::Speed, "speed", "Speed"),
        DIR,
        value(Kind::Length, "depth", "Depth"),
        SIDE,
        value(Kind::Length, "length", "Length"),
        GAP,
        value(Kind::Count, "passes", "Passes"),
    };

    constexpr Cell ELLIPSE[] = {
        value(Kind::Feed, "pitch", "Feed"),
        toggle("concave", "Convex", "off", "Concave", "on"),
        value(Kind::Length, "depth", "Depth"),
        DIR,
        value(Kind::Length, "length", "Length"),
        SPINDLE,
        value(Kind::Count, "passes", "Passes"),
    };

    struct ModeLayout
    {
        const char *label;
        const Cell *cells;
        size_t count;
    };

    template <size_t N>
    constexpr ModeLayout layout(const char *label, const Cell (&cells)[N])
    {
        static_assert(N <= ModeCells::MAX, "too many cells");
        return {label, cells, N};
    }

    constexpr Cell PROBE[] = {
        value(Kind::Length, "probe-dst", "Distance"),
        value(Kind::Length, "offset", "Offset"),
    };

    constexpr ModeLayout LAYOUTS[Mode::COUNT] = {
        layout("Gear", GEAR),
        layout("Async", ASYNC),
        layout("Turn", TURN),
        layout("Thread", THREAD),
        layout("Part", PART),
        layout("Slot", SLOT),
        layout("Ellipse", ELLIPSE),
        {"G-code", nullptr, 0},
        layout("Probe", PROBE),
    };
}

namespace ModeCells
{
    const char *label(Mode::Id id)
    {
        return LAYOUTS[(size_t)id].label;
    }

    const Cell *cells(Mode::Id id, size_t &count)
    {
        count = LAYOUTS[(size_t)id].count;
        return LAYOUTS[(size_t)id].cells;
    }
}
