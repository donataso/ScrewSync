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
#include "Stepper.hpp"
#include "StepperRamp.hpp"
#include "StepperLimits.hpp"
#include "Settings.hpp"
#include "Machine.hpp"
#include <zephyr/device.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/counter.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <initializer_list>

LOG_MODULE_REGISTER(stepper, LOG_LEVEL_INF);

// Step pulses are bit-banged from each axis's own GPTimer alarm ISR rather
// than a PWM peripheral: the ramp needs a freshly computed, different
// period on every single step, which a PWM channel's slowly-updated duty
// register can't give. Z uses timer2, X uses timer3 (TIMG1 T0/T1, prescaled
// to a 1MHz/1us tick in app.overlay -- TIMG0 is reserved for the Wi-Fi
// stack's RTC slow-clock calibration), so the two axes never contend for
// the same hardware alarm.

namespace
{
    constexpr uint32_t STEP_PULSE_US = 3;
    constexpr uint32_t DIRECTION_SETUP_DELAY_US = 10;

    struct AxisState
    {
        gpio_dt_spec step;
        gpio_dt_spec dir;
        gpio_dt_spec ena;
        const struct device *timer;
        Settings::Axis *cfg;

        struct k_spinlock lock;
        int64_t targetPos;
        int64_t currentPos;
        int64_t motorPos; // actual stepper shaft position; trails currentPos by
                           // up to backlashSteps right after a direction reversal
        int32_t currentSpeed;  // steps/sec
        int32_t speedOverride; // steps/sec, 0 = use cfg->speedManualMove
        int32_t moveMaxSpeed;  // steps/sec, 0 = speedOverride/speedManualMove
        bool dirLevel; // last direction written (true = positive)
        bool dirValid;
        bool running;
        bool holdRequested;
        bool disabled;
    };

    AxisState axisZ = {
        GPIO_DT_SPEC_GET(DT_PATH(zephyr_user), z_step_gpios),
        GPIO_DT_SPEC_GET(DT_PATH(zephyr_user), z_dir_gpios),
        GPIO_DT_SPEC_GET(DT_PATH(zephyr_user), z_ena_gpios),
        nullptr, &Settings::active.z, {}, 0, 0, 0, 0, 0, 0, false, false,
        false, false, false,
    };
    AxisState axisX = {
        GPIO_DT_SPEC_GET(DT_PATH(zephyr_user), x_step_gpios),
        GPIO_DT_SPEC_GET(DT_PATH(zephyr_user), x_dir_gpios),
        GPIO_DT_SPEC_GET(DT_PATH(zephyr_user), x_ena_gpios),
        nullptr, &Settings::active.x, {}, 0, 0, 0, 0, 0, 0, false, false,
        false, false, false,
    };

    AxisState &stateOf(Stepper::Axis axis)
    {
        return axis == Stepper::Axis::Z ? axisZ : axisX;
    }

    // Caller holds s.lock. The one place that decides whether the axis
    // should be energized: whenever the manual switch has it enabled.
    bool shouldHold(const AxisState &s)
    {
        return !s.disabled;
    }

    void reconcile(AxisState &s)
    {
        gpio_pin_set_dt(&s.ena, shouldHold(s) != s.cfg->invertEnable);
    }

    int32_t maxSpeedOf(const AxisState &s)
    {
        if (s.moveMaxSpeed != 0) {
            return s.moveMaxSpeed;
        }
        return s.speedOverride != 0 ? s.speedOverride : s.cfg->speedManualMove;
    }

    int32_t startSpeedOf(const AxisState &s)
    {
        int32_t maxSpeed = maxSpeedOf(s);
        return s.cfg->speedStart < maxSpeed ? s.cfg->speedStart : maxSpeed;
    }

    void armNext(AxisState &s, uint32_t periodUs);

    // Caller holds s.lock.
    int64_t moveToLocked(AxisState &s, int64_t targetSteps, int32_t maxSpeed)
    {
        // The one choke point for every mover (Jog/Sync both drive through
        // here, same as the travel clamp below) -- while flashing/estopped
        // or manually disabled, pin the target to the current position
        // instead of accepting a new one, so an in-flight ramp sees zero
        // distance remaining and stops on its very next tick.
        Machine::State ms = Machine::state();
        if (ms == Machine::State::Flashing || ms == Machine::State::EStop || s.disabled)
        {
            s.targetPos = s.currentPos;
            return s.targetPos;
        }

        s.moveMaxSpeed = maxSpeed < 0 ? 0 : maxSpeed;
        int64_t maxSteps = Stepper::maxTravelSteps(s.cfg->maxTravelMm, s.cfg->motorSteps, s.cfg->screwPitchDu);
        targetSteps = Stepper::clampToTravel(targetSteps, maxSteps);
        s.targetPos = targetSteps;
        if (!s.running && targetSteps != s.currentPos) {
            s.running = true;
            s.currentSpeed = startSpeedOf(s);
            s.dirValid = false;
            reconcile(s);
            armNext(s, 1000000 / s.currentSpeed);
        }
        return targetSteps;
    }

    // Caller holds s.lock. 0 when idle.
    int movingDirection(const AxisState &s)
    {
        if (!s.running || s.targetPos == s.currentPos) {
            return 0;
        }
        return s.targetPos > s.currentPos ? 1 : -1;
    }

    // Caller holds s.lock.
    void stopLocked(AxisState &s)
    {
        int direction = movingDirection(s);
        if (direction == 0) {
            return;
        }
        int64_t distance = Stepper::stoppingDistance(s.currentSpeed, s.cfg->acceleration);
        moveToLocked(s, s.currentPos + direction * distance, s.moveMaxSpeed);
    }

    void armNext(AxisState &s, uint32_t periodUs)
    {
        struct counter_alarm_cfg cfg = {};
        cfg.callback = [](const struct device *, uint8_t, uint32_t, void *userData) {
            AxisState &s = *static_cast<AxisState *>(userData);
            k_spinlock_key_t key = k_spin_lock(&s.lock);

            int64_t delta = s.targetPos - s.currentPos;
            if (delta == 0) {
                s.running = false;
                reconcile(s);
                k_spin_unlock(&s.lock, key);
                return;
            }

            bool positive = delta > 0;
            if (!s.dirValid || positive != s.dirLevel) {
                gpio_pin_set_dt(&s.dir, positive != s.cfg->invertDirection);
                s.dirLevel = positive;
                s.dirValid = true;
                s.currentSpeed = startSpeedOf(s); // no "flying" direction change
                k_busy_wait(DIRECTION_SETUP_DELAY_US);
            }

            gpio_pin_set_dt(&s.step, 1);
            k_busy_wait(STEP_PULSE_US);
            gpio_pin_set_dt(&s.step, 0);
            if (positive) {
                if (s.motorPos >= s.currentPos) {
                    s.currentPos++;
                }
                s.motorPos++;
            } else {
                int64_t backlash = Stepper::backlashSteps(s.cfg->backlashDu, s.cfg->motorSteps, s.cfg->screwPitchDu);
                if (s.motorPos <= s.currentPos - backlash) {
                    s.currentPos--;
                }
                s.motorPos--;
            }

            int64_t distRemaining = positive ? (s.targetPos - s.currentPos) : (s.currentPos - s.targetPos);
            if (distRemaining == 0) {
                s.running = false;
                reconcile(s);
                k_spin_unlock(&s.lock, key);
                return;
            }

            s.currentSpeed = Stepper::nextRampSpeed(s.currentSpeed, distRemaining, s.cfg->acceleration,
                                                    s.cfg->speedStart, maxSpeedOf(s));

            armNext(s, 1000000 / s.currentSpeed);
            k_spin_unlock(&s.lock, key);
        };
        cfg.ticks = periodUs;
        cfg.user_data = &s;
        cfg.flags = 0;
        counter_set_channel_alarm(s.timer, 0, &cfg);
    }
}

namespace Stepper
{
    void begin()
    {
        axisZ.timer = DEVICE_DT_GET(DT_CHILD(DT_NODELABEL(timer2), counter));
        axisX.timer = DEVICE_DT_GET(DT_CHILD(DT_NODELABEL(timer3), counter));

        for (AxisState *s : {&axisZ, &axisX})
        {
            if (!gpio_is_ready_dt(&s->step) || !gpio_is_ready_dt(&s->dir) || !gpio_is_ready_dt(&s->ena)) {
                LOG_ERR("stepper GPIOs not ready");
                continue;
            }
            gpio_pin_configure_dt(&s->step, GPIO_OUTPUT_INACTIVE);
            gpio_pin_configure_dt(&s->dir, GPIO_OUTPUT_INACTIVE);
            gpio_pin_configure_dt(&s->ena, GPIO_OUTPUT_INACTIVE);

            if (!device_is_ready(s->timer)) {
                LOG_ERR("stepper timer not ready");
                continue;
            }
            counter_start(s->timer);

            // setDisabled() re-runs this with the state restored from NVS
            // right after begin(), before any move.
            reconcile(*s);
        }
    }

    void setEnabled(Axis axis, bool enabled)
    {
        AxisState &s = stateOf(axis);
        gpio_pin_set_dt(&s.ena, enabled != s.cfg->invertEnable);
    }

    void setHoldRequested(Axis axis, bool requested)
    {
        AxisState &s = stateOf(axis);
        k_spinlock_key_t key = k_spin_lock(&s.lock);
        s.holdRequested = requested;
        reconcile(s);
        k_spin_unlock(&s.lock, key);
    }

    void setDisabled(Axis axis, bool disabled)
    {
        AxisState &s = stateOf(axis);
        k_spinlock_key_t key = k_spin_lock(&s.lock);
        s.disabled = disabled;
        if (disabled) {
            s.targetPos = s.currentPos;
        }
        reconcile(s);
        k_spin_unlock(&s.lock, key);
    }

    bool isDisabled(Axis axis)
    {
        AxisState &s = stateOf(axis);
        k_spinlock_key_t key = k_spin_lock(&s.lock);
        bool disabled = s.disabled;
        k_spin_unlock(&s.lock, key);
        return disabled;
    }

    bool isHeld(Axis axis)
    {
        AxisState &s = stateOf(axis);
        k_spinlock_key_t key = k_spin_lock(&s.lock);
        bool held = shouldHold(s);
        k_spin_unlock(&s.lock, key);
        return held;
    }

    int64_t moveTo(Axis axis, int64_t targetSteps, int32_t maxSpeed)
    {
        AxisState &s = stateOf(axis);
        k_spinlock_key_t key = k_spin_lock(&s.lock);
        int64_t accepted = moveToLocked(s, targetSteps, maxSpeed);
        k_spin_unlock(&s.lock, key);
        return accepted;
    }

    bool arrived(Axis axis)
    {
        AxisState &s = stateOf(axis);
        k_spinlock_key_t key = k_spin_lock(&s.lock);
        bool atTarget = !s.running && s.currentPos == s.targetPos;
        k_spin_unlock(&s.lock, key);
        return atTarget;
    }

    void moveBy(Axis axis, int64_t deltaSteps)
    {
        AxisState &s = stateOf(axis);
        k_spinlock_key_t key = k_spin_lock(&s.lock);
        int64_t base = s.running ? s.targetPos : s.currentPos;
        moveToLocked(s, base + deltaSteps, 0);
        k_spin_unlock(&s.lock, key);
    }

    void runAhead(Axis axis, int direction, uint32_t horizonUs)
    {
        AxisState &s = stateOf(axis);
        k_spinlock_key_t key = k_spin_lock(&s.lock);
        int moving = movingDirection(s);
        if (moving == -direction)
        {
            stopLocked(s);
            k_spin_unlock(&s.lock, key);
            return;
        }
        int32_t speed = moving != 0 ? s.currentSpeed : s.cfg->speedStart;
        int64_t ahead = s.currentPos + direction * runAheadDistance(speed, s.cfg->acceleration, maxSpeedOf(s), horizonUs);
        bool alreadyFurther = moving != 0 && (direction > 0 ? s.targetPos >= ahead : s.targetPos <= ahead);
        if (!alreadyFurther) {
            moveToLocked(s, ahead, 0);
        }
        k_spin_unlock(&s.lock, key);
    }

    void stop(Axis axis)
    {
        AxisState &s = stateOf(axis);
        k_spinlock_key_t key = k_spin_lock(&s.lock);
        stopLocked(s);
        k_spin_unlock(&s.lock, key);
    }

    int64_t haltNow(Axis axis)
    {
        AxisState &s = stateOf(axis);
        k_spinlock_key_t key = k_spin_lock(&s.lock);
        s.targetPos = s.currentPos;
        int64_t pos = s.currentPos;
        k_spin_unlock(&s.lock, key);
        return pos;
    }

    int64_t position(Axis axis)
    {
        AxisState &s = stateOf(axis);
        k_spinlock_key_t key = k_spin_lock(&s.lock);
        int64_t pos = s.currentPos;
        k_spin_unlock(&s.lock, key);
        return pos;
    }

    void zero(Axis axis, int64_t positionSteps)
    {
        AxisState &s = stateOf(axis);
        k_spinlock_key_t key = k_spin_lock(&s.lock);
        int64_t delta = s.currentPos - positionSteps;
        s.currentPos = positionSteps;
        s.motorPos -= delta;
        s.targetPos -= delta;
        k_spin_unlock(&s.lock, key);
    }

    bool isMoving(Axis axis)
    {
        AxisState &s = stateOf(axis);
        k_spinlock_key_t key = k_spin_lock(&s.lock);
        bool running = s.running;
        k_spin_unlock(&s.lock, key);
        return running;
    }

    void setSpeedOverride(Axis axis, int32_t speedStepsPerSec)
    {
        AxisState &s = stateOf(axis);
        k_spinlock_key_t key = k_spin_lock(&s.lock);
        s.speedOverride = speedStepsPerSec;
        k_spin_unlock(&s.lock, key);
    }
}
