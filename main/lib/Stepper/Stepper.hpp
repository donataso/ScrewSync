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
#ifndef Stepper_h
#define Stepper_h

#include <stdint.h>

namespace Stepper
{
    enum class Axis
    {
        Z,
        X,
    };

    void begin();

    // Raw ena GPIO write -- used only by reconcile() internally and by
    // Machine's EStop halt (a deliberate one-way bypass, since EStop always
    // reboots right after). Everything else goes through setHoldRequested/
    // setDisabled below so the two never fight over the pin.
    void setEnabled(Axis axis, bool enabled);

    // Keeps the axis energized independent of whether it's currently
    // stepping -- a running mode requests this on the axes it owns, so it
    // holds continuously, not just for the duration of each correction move.
    void setHoldRequested(Axis axis, bool requested);

    // The persisted manual enable/disable switch -- a hard veto: the axis
    // de-energizes immediately regardless of holdRequested or an in-flight
    // move, and stays off until re-enabled. See RunSettings for the
    // persisted/boot-restored form.
    void setDisabled(Axis axis, bool disabled);
    bool isDisabled(Axis axis);

    // True while the axis is actually energized (held for a mode, mid-move,
    // or both), false while released or manually disabled.
    bool isHeld(Axis axis);

    // Requests the axis move to targetSteps (motor steps from power-on
    // zero), ramped per Settings::active.{z,x} acceleration/speedStart.
    // maxSpeed caps this move, 0 = speedManualMove (or the override).
    // Safe to call again mid-move to redirect the target. Returns the
    // accepted target: differs when travel-clamped or refused.
    int64_t moveTo(Axis axis, int64_t targetSteps, int32_t maxSpeed = 0);

    // At rest on the last accepted target.
    bool arrived(Axis axis);

    // Shifts the target by deltaSteps: from the current target while
    // moving (queued taps add up), else from the current position.
    void moveBy(Axis axis, int64_t deltaSteps);

    // Hold-to-move: keeps the target far enough ahead in direction (+1/-1)
    // to keep accelerating for horizonUs; never pulls it back. Stops
    // instead while moving the other way. The axis stops by itself once
    // calls stop coming.
    void runAhead(Axis axis, int direction, uint32_t horizonUs);

    void stop(Axis axis);

    // Immediate stop, no decel -- ISR-safe. Returns the halted position,
    // the latch point for the probe's contact interrupt.
    int64_t haltNow(Axis axis);

    int64_t position(Axis axis);
    bool isMoving(Axis axis);

    // Rebases current/target position so the axis's present position
    // becomes positionSteps -- an absolute origin shift, not a move.
    void zero(Axis axis, int64_t positionSteps = 0);

    // Runtime max-speed override for moveTo() (e.g. a touchscreen jog-speed
    // slider): 0 clears it, falling back to Settings::active.{z,x}'s
    // configured speedManualMove. Not persisted -- deliberately separate
    // from that configured default rather than overwriting it.
    void setSpeedOverride(Axis axis, int32_t speedStepsPerSec);
}

#endif
