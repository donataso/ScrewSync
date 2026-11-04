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
#ifndef Mode_h
#define Mode_h

#include "Stepper.hpp"
#include <stddef.h>
#include <stdint.h>

// Selected operating mode, start/stop, and the per-mode parameters
// (docs/screen.md "Modes"). Parameters persist via RunSettings. One runner
// thread drives the running mode; axes it drives are owned by it.
namespace Mode
{
    enum class Id : uint8_t
    {
        Gear,
        Async,
        Turn,
        Thread,
        Part,
        Slot,
        Ellipse,
        Gcode,
        Probe,
    };
    constexpr size_t COUNT = 9;

    struct Gear
    {
        int32_t pitchDu;
        bool reverse; // feeds +Z/+X in forward rotation
        Stepper::Axis axis;
        int32_t stopAfterDu; // from the engage point, 0 = off
        float taper;         // axis Z only: X follows, + = diameter grows toward the chuck
    };

    struct Progress
    {
        const char *phase; // "" when idle
        int32_t pass;
        int32_t passes;
        bool waiting; // paused until the next start
        uint32_t step; // Gcode: motion steps executed so far
    };

    // Loads parameters and starts the runner. After RunSettings::load().
    void begin();

    const char *name(Id id);
    bool parse(const char *name, Id &out);

    Id current();
    bool running();
    bool owns(Stepper::Axis axis);

    // Each returns nullptr on success, else the reason it was refused.
    const char *select(Id id);
    // fromZero: rapid Z then X to 0 first, and start the op there.
    const char *start(bool fromZero = false);
    const char *skip();

    // Feed override of the running job, percent of the set feed. Every mode
    // but thread; back to 100 on each start.
    constexpr int32_t OVERRIDE_MIN_PCT = 50;
    constexpr int32_t OVERRIDE_MAX_PCT = 150;
    constexpr int32_t OVERRIDE_STEP_PCT = 10;
    bool hasFeedOverride(Id id);
    bool canSkip(Id id);
    bool startsAtZero(Id id);
    // Start off zero asks to confirm, without offering to go to zero.
    bool confirmsOffZero(Id id);

    enum class StartPrompt : uint8_t
    {
        None,
        OfferZero,      // startsAtZero()
        ConfirmOffZero, // confirmsOffZero()
    };
    // What starting the selected mode away from Z/X 0 asks first; None while running or at 0.
    StartPrompt startPrompt();
    int32_t feedOverride();
    // Clamped to the range.
    const char *setFeedOverride(int32_t pct);

    const char *setParam(Id id, const char *param, const char *value);
    // Sets every "name=value" of a describe() line at once, or only checks
    // them unless commit. Refused while that mode runs.
    const char *loadParams(Id id, const char *line, bool commit);
    bool required(Id id, const char *param);
    // All required params of the selected mode are set.
    bool ready();
    // "name=value ..." for every parameter; false for an unimplemented mode.
    bool describe(Id id, char *out, size_t outMax);

    // Fills thread pitch, depth and taper from the thread table.
    const char *threadPreset(const char *group, const char *designation);

    // Selects probe mode and starts its cycle on the given axis/direction
    // in one call (the display's/protocol's "probe go z-|z+|x-|x+").
    const char *probeGo(Stepper::Axis axis, bool reverse);
    // The probe input's contact ISR; see ProbeInput::begin().
    void probeEdge(bool contactNow);

    Gear gear();
    Progress progress();

    // Why the last job ended on its own ("" if it finished normally).
    const char *lastError();

    // Controlled stop: disengages the mode and decelerates both axes.
    void stop();

    // Stops the running mode if it drives this axis.
    void onAxisDisabled(Stepper::Axis axis);
}

#endif
