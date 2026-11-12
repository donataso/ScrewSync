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
#ifndef ModeInternal_h
#define ModeInternal_h

#include "Mode.hpp"
#include "Sync.hpp"
#include <stddef.h>
#include <stdint.h>

// Shared by the mode implementations. Everything here runs with the mode
// lock held: Mode's public API takes it, the runner holds it per tick.
//
// Directions (display jog arrows): -X toward the centerline, -Z toward the
// chuck; the workpiece is at -X/-Z. Spindle counts increase in forward
// rotation. Pitches and speeds are magnitudes; a "reverse" flag (dir +,
// hand lh) turns the feed from -Z/-X to +Z/+X. Spindle-fed modes cut in
// the rotation their "spindle" param names and stop on the other. Tapers
// are diameter change per length, + = growing toward the chuck, whatever
// the feed direction.
namespace Mode::detail
{
    enum class Kind : uint8_t
    {
        Du,    // int32_t deci-microns; "off" = 0 when 0 is in range
        Count, // int32_t
        Axis,  // Stepper::Axis, "z"|"x"
        Flag,  // bool, "off"|"on" or Param::choices
        Angle, // float ratio, diameter change per length; set and shown as half-angle degrees
        Text,  // char[max], read-only
    };

    struct Param
    {
        const char *name;
        Kind kind;
        size_t offset;
        double min; // stored value
        double max;
        bool live;                     // may change while the mode runs
        bool required = false;         // Du: 0 = not set, start refused
        const char *choices = nullptr; // Flag: "false|true" names
    };

    struct Ops
    {
        bool needsX;
        void *params;
        size_t paramsSize;
        const Param *paramTable;
        size_t paramCount;
        // Sets up and claims the mode's axes; nullptr on success.
        const char *(*start)(int64_t spindlePos);
        void (*tick)(int64_t spindlePos);
        // Optional. A live param changed while running (params already
        // updated); a refusal reverts it.
        const char *(*onLiveChange)(const void *previous);
        // Optional. Called after any param change, running or not.
        void (*onChange)(const Param &param);
        void (*progress)(Progress &out);
        void (*skip)();
        // Spindle-fed modes: the "spindle" param, true = reverse rotation.
        const bool *spindleReverse;
        // Optional. Continues a job waiting for start; false if it isn't waiting.
        bool (*resume)();
    };

    extern const Ops GEAR;
    extern const Ops ASYNC;
    extern const Ops TURN;
    extern const Ops THREAD;
    extern const Ops PART;
    extern const Ops SLOT;
    extern const Ops ELLIPSE;
    extern const Ops GCODE;
    extern const Ops PROBE;

    void lock();
    void unlock();

    struct Guard
    {
        Guard() { lock(); }
        ~Guard() { unlock(); }
    };

    // nullptr for an unimplemented mode.
    const Ops *ops(Id id);
    bool isRunning(Id id);

    const Param *firstMissing(const Ops &ops);

    void saveParams(Id id);

    const char *claim(Stepper::Axis axis);

    // Ends the running mode from its own tick (job done or failed).
    void finish(const char *reason = nullptr);

    int64_t duToSteps(Stepper::Axis axis, double du);
    double stepsToDu(Stepper::Axis axis, int64_t steps);
    int32_t rapidSpeed(Stepper::Axis axis);
    int32_t duPerMinToStepsPerSec(Stepper::Axis axis, int32_t duPerMin);
    int32_t countsPerRev();

    int32_t overridden(int32_t feed);
    int32_t feedOverridePct();

    // 1 when the running job cuts in forward rotation, else -1.
    int spindleSign();

    // Spindle travel in the job's cutting rotation from one count to a later one.
    inline int64_t spindleTravel(int64_t from, int64_t to) { return spindleSign() * (to - from); }

    // Step direction of a positive-forward feed.
    inline int feedDirection(int64_t pitchOrSpeed) { return pitchOrSpeed > 0 ? -1 : 1; }

    inline int32_t signedDu(int32_t magnitude, bool reverse) { return reverse ? -magnitude : magnitude; }

    // Drives the axis toward target at speed (0 = rapid). True once at rest
    // there; a travel-clamped target ends the mode.
    bool moveAndWait(Stepper::Axis axis, int64_t target, int32_t speed = 0);

    // Threads share one helix: through axis 0 at spindle angle 0, so reruns
    // and later passes land in the same groove.
    // A spindle count where link puts refAxisPos on that helix.
    int64_t helixPhase(const Sync::Link &link, int64_t refAxisPos);
    // The first such count at or ahead of spindlePos in the cutting
    // rotation, shifted aheadCounts further along it.
    int64_t helixOrigin(const Sync::Link &link, int64_t refAxisPos, int64_t aheadCounts, int64_t spindlePos);

    // Feed per spindle rev integrated tick by tick, so a live pitch change
    // doesn't jump the position. Clamped between from and to.
    struct SpindleFeed
    {
        Stepper::Axis axis;
        double pos;
        int64_t from;
        int64_t to;
        int64_t lastSpindle;

        void begin(Stepper::Axis feedAxis, int64_t fromSteps, int64_t toSteps, int64_t spindlePos);
        // True once arrived at to.
        bool tick(int64_t spindlePos, int32_t pitchDu);
    };
}

#endif
