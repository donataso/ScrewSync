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

    constexpr Cell cycle(const char *param, const char *label, const char *values, const char *texts)
    {
        return {Kind::Cycle, param, label, texts, values, nullptr, nullptr, nullptr};
    }

    constexpr Cell button(const char *label, const char *command)
    {
        return {Kind::Button, nullptr, label, nullptr, command, nullptr, nullptr, nullptr};
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
        GAP,
        GAP,
        GAP,
        GAP,
        value(Kind::Count, "passes", "Passes"),
        cycle("spindle", "Spindle", "fwd|rev", "Fwd|Rev"),
        button("More", "ui page 1"),
    };

    constexpr Cell TURN_FINISH[] = {
        value(Kind::Length, "finish", "Finish"),
        value(Kind::Feed, "finish-feed", "Fin. feed"),
        toggle("spring", "No spring", "off", "Spring", "on"),
        GAP,
        GAP,
        GAP,
        GAP,
        GAP,
        GAP,
        GAP,
        GAP,
        GAP,
        button("Back", "ui page 0"),
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
        cycle("flank", "Flank", "off|on|side", "off|\\/|/"),
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
        const Cell *moreCells;
        size_t moreCount;
    };

    template <size_t N>
    constexpr ModeLayout layout(const char *label, const Cell (&cells)[N])
    {
        static_assert(N <= ModeCells::MAX, "too many cells");
        return {label, cells, N, nullptr, 0};
    }

    template <size_t N, size_t M>
    constexpr ModeLayout layout(const char *label, const Cell (&cells)[N], const Cell (&moreCells)[M])
    {
        static_assert(N <= ModeCells::MAX && M <= ModeCells::MAX, "too many cells");
        return {label, cells, N, moreCells, M};
    }

    // Settings fields: the probe panel is not a mode.
    constexpr Cell PROBE[] = {
        value(Kind::Length, "probeDstDu", "Distance"),
        value(Kind::Length, "probeOffsetDu", "Offset"),
    };

    constexpr ModeLayout LAYOUTS[Mode::COUNT] = {
        layout("Gear", GEAR),
        layout("Async", ASYNC),
        layout("Turn", TURN, TURN_FINISH),
        layout("Thread", THREAD),
        layout("Part", PART),
        layout("Slot", SLOT),
        layout("Ellipse", ELLIPSE),
        {"G-code", nullptr, 0, nullptr, 0},
        layout("Probe", PROBE),
    };
}

namespace ModeCells
{
    const char *label(Mode::Id id)
    {
        return LAYOUTS[(size_t)id].label;
    }

    size_t pages(Mode::Id id)
    {
        return LAYOUTS[(size_t)id].moreCells ? 2 : 1;
    }

    const Cell *cells(Mode::Id id, size_t page, size_t &count)
    {
        const ModeLayout &l = LAYOUTS[(size_t)id];
        count = page ? l.moreCount : l.count;
        return page ? l.moreCells : l.cells;
    }
}
