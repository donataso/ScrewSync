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
#include "ModeSteps.hpp"
#include "ModeInternal.hpp"
#include "ModeMath.hpp"
#include "Events.hpp"
#include "Machine.hpp"
#include "Mode.hpp"
#include "Motion.hpp"
#include "ProbeInput.hpp"
#include "Rpm.hpp"
#include "Settings.hpp"
#include "SettingsFields.hpp"
#include "Wcs.hpp"
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <zephyr/sys/util.h>
#include <initializer_list>

namespace
{
    using namespace Mode::Detail;

    double axisDeltaDu(Stepper::Axis axis, int32_t targetDu)
    {
        return (double)(targetDu - Wcs::position(axis));
    }

    int64_t machineSteps(Stepper::Axis axis, const Gcode::Step &s)
    {
        return Machine::toSteps(axis, Wcs::toMachine(axis, axis == Stepper::Axis::Z ? s.zDu : s.xDu));
    }

    const char *clampReason(Stepper::Axis axis)
    {
        return Stepper::isDisabled(axis) ? "axis disabled" : "travel limit";
    }

    enum class Watch : uint8_t
    {
        None,
        Contact,
        Release,
    };

    // Shared with Mode::probeEdge() (ISR context): plain flags, no mode
    // lock -- the lock is a k_mutex, not ISR-safe.
    volatile Watch g_watch = Watch::None;
    volatile bool g_latched = false;
    volatile int64_t g_latchedPos = 0;
    volatile int64_t g_latchedMotor = 0;
    Stepper::Axis g_probeAxis = Stepper::Axis::Z;

    void publishProbe(StepMotion &m, Stepper::Axis axis, bool found, int64_t pos, int64_t motor)
    {
        Events::ProbeResult r = {(uint8_t)axis, found, Machine::positionOf(axis, pos) - Wcs::offset(Wcs::active(), axis),
                                 Machine::positionOf(axis, motor)};
        m.probeFound = found;
        m.probeTripDu = (int32_t)r.posDu;
        m.probeMotorDu = (int32_t)r.motorDu;
        Events::publish(&PROBE_CHAN, &r);
    }

    struct SettingsSource : Gcode::ParamSource
    {
        bool get(const char *name, double &value, bool &length, bool &live) override
        {
            live = false;
            if (!strcmp(name, "_probe_contact")) {
                value = ProbeInput::contact() ? 1 : 0;
                length = false;
                live = true;
                return true;
            }
            return Settings::paramValue(name, Settings::g_active, value, length);
        }
    };

    Gcode::Params g_params;
    SettingsSource g_source;

    // True once the move ends. The target is commanded one time: a second
    // moveTo() would undo the halt of the interrupt.
    bool driveProbe(StepMotion &m, const Gcode::Step &s, const char *&error)
    {
        bool away = s.probe & Gcode::PROBE_AWAY;
        Stepper::Axis axis = m.mainAxis;
        if (!m.probeStarted) {
            g_latched = false;
            g_probeAxis = axis;
            g_watch = away ? Watch::Release : Watch::Contact;
            if (ProbeInput::contact() != away) {
                error = away ? "no contact" : "contact present";
            } else if (Stepper::moveTo(axis, m.mainTo, duPerMinToStepsPerSec(axis, s.feed)) != m.mainTo) {
                error = clampReason(axis);
            }
            m.probeStarted = true;
            return false;
        }
        if (g_latched) {
            g_latched = false;
            if (ProbeInput::contact() == away) {
                error = away ? "release glitch" : "contact glitch";
                return false;
            }
            publishProbe(m, axis, true, g_latchedPos, g_latchedMotor);
            return true;
        }
        if (!Stepper::arrived(axis) || Stepper::position(axis) != m.mainTo) {
            return false;
        }
        g_watch = Watch::None;
        if (g_latched) {
            return false;
        }
        if (s.probe & Gcode::PROBE_NO_ERROR) {
            publishProbe(m, axis, false, Stepper::position(axis), Stepper::motorPosition(axis));
            return true;
        }
        error = away ? "no release" : "no contact";
        return false;
    }

    // True once aux reaches its target.
    bool followAux(StepMotion &m, const char *&error)
    {
        if (m.auxTo == m.auxFrom) {
            return true;
        }
        double fraction = (double)(Stepper::position(m.mainAxis) - m.mainFrom) / (double)(m.mainTo - m.mainFrom);
        int64_t target = m.auxFrom + (int64_t)llround(fraction * (double)(m.auxTo - m.auxFrom));
        if (Stepper::moveTo(m.auxAxis, target, rapidSpeed(m.auxAxis)) != target) {
            error = clampReason(m.auxAxis);
            return false;
        }
        return target == m.auxTo && Stepper::arrived(m.auxAxis) && Stepper::position(m.auxAxis) == m.auxTo;
    }
}

namespace Mode::Detail
{
    bool isMotion(Gcode::StepKind kind)
    {
        return kind == Gcode::StepKind::Rapid || kind == Gcode::StepKind::Feed || kind == Gcode::StepKind::Thread ||
               kind == Gcode::StepKind::Probe;
    }

    void probeDisarm()
    {
        g_watch = Watch::None;
        g_latched = false;
    }

    Gcode::Params &params()
    {
        return g_params;
    }

    Gcode::ParamSource &paramSource()
    {
        return g_source;
    }

    Gcode::WcsState wcsState()
    {
        static_assert(Wcs::COUNT == Gcode::WCS_COUNT);
        Gcode::WcsState state = {};
        state.active = (uint8_t)Wcs::active();
        for (size_t i = 0; i < Wcs::COUNT; i++) {
            state.offsetDu[i][0] = (int32_t)Wcs::offset(i, Stepper::Axis::Z);
            state.offsetDu[i][1] = (int32_t)Wcs::offset(i, Stepper::Axis::X);
        }
        return state;
    }

    const char *applyMachineStep(const Gcode::Step &s)
    {
        if (s.kind == Gcode::StepKind::SpindleAngle) {
            Motion::setSpindleAngle(s.feed / 1000.0);
            return nullptr;
        }
        if (s.kind == Gcode::StepKind::SelectWcs) {
            return Wcs::select(s.wcs);
        }
        for (Stepper::Axis axis : {Stepper::Axis::Z, Stepper::Axis::X}) {
            bool isZ = axis == Stepper::Axis::Z;
            if (!(s.axes & (isZ ? Gcode::AXIS_Z : Gcode::AXIS_X))) {
                continue;
            }
            int32_t value = isZ ? s.zDu : s.xDu;
            if (s.kind == Gcode::StepKind::SetPosition) {
                const char *refusal = Wcs::setPosition(s.wcs, axis, value);
                if (refusal) {
                    return refusal;
                }
            }
        }
        return nullptr;
    }

    void StepMotion::begin(const Gcode::Step &s, int64_t spindlePos)
    {
        double dz = axisDeltaDu(Stepper::Axis::Z, s.zDu);
        double dx = axisDeltaDu(Stepper::Axis::X, s.xDu);
        mainAxis = fabs(dz) >= fabs(dx) ? Stepper::Axis::Z : Stepper::Axis::X;
        auxAxis = mainAxis == Stepper::Axis::Z ? Stepper::Axis::X : Stepper::Axis::Z;
        mainFrom = Stepper::position(mainAxis);
        auxFrom = Stepper::position(auxAxis);
        mainTo = machineSteps(mainAxis, s);
        auxTo = machineSteps(auxAxis, s);
        // A sub-step du delta can round the chosen main axis to zero steps;
        // fall back to whichever axis actually has somewhere to go.
        if (mainTo == mainFrom && auxTo != auxFrom) {
            Stepper::Axis axis = mainAxis;
            mainAxis = auxAxis;
            auxAxis = axis;
            int64_t v = mainFrom;
            mainFrom = auxFrom;
            auxFrom = v;
            v = mainTo;
            mainTo = auxTo;
            auxTo = v;
        }
        if (s.kind == Gcode::StepKind::Thread) {
            int32_t dupr = (mainTo > mainFrom ? -1 : 1) * abs(s.feed);
            thread = {mainAxis, dupr, 1, 0, mainFrom};
            thread.originSpindlePos = helixOrigin(thread, mainFrom, 0, spindlePos);
        } else if (s.kind == Gcode::StepKind::Feed) {
            vectorDu = sqrt(dz * dz + dx * dx);
        }
        probeStarted = false;
    }

    bool StepMotion::drive(const Gcode::Step &s, int64_t spindlePos, bool spindleGated, const char *&error)
    {
        error = nullptr;
        if (s.kind == Gcode::StepKind::Probe) {
            return driveProbe(*this, s, error);
        }
        if (s.kind == Gcode::StepKind::Thread) {
            int64_t target = thread.target(spindlePos);
            target = CLAMP(target, MIN(mainFrom, mainTo), MAX(mainFrom, mainTo));
            if (Stepper::moveTo(mainAxis, target, rapidSpeed(mainAxis)) != target) {
                error = clampReason(mainAxis);
                return false;
            }
            bool mainDone = target == mainTo && Stepper::arrived(mainAxis) && Stepper::position(mainAxis) == mainTo;
            bool auxDone = followAux(*this, error);
            return !error && mainDone && auxDone;
        }
        if (spindleGated && Rpm::value() == 0) {
            Stepper::stop(mainAxis);
            Stepper::stop(auxAxis);
            return false;
        }
        int32_t cap = rapidSpeed(mainAxis);
        int32_t speed = cap;
        if (s.kind != Gcode::StepKind::Rapid) {
            speed = vectorFeedStepsPerSec(vectorDu, overridden(s.feed), mainTo - mainFrom, cap);
        }
        if (Stepper::moveTo(mainAxis, mainTo, speed) != mainTo) {
            error = clampReason(mainAxis);
            return false;
        }
        bool mainDone = Stepper::arrived(mainAxis) && Stepper::position(mainAxis) == mainTo;
        bool auxDone = followAux(*this, error);
        return !error && mainDone && auxDone;
    }
}

namespace Mode
{
    // ISR context (ProbeInput's GPIO callback) -- no mode lock.
    void probeEdge(bool contactNow)
    {
        Watch watch = g_watch;
        bool wanted = contactNow ? watch == Watch::Contact : watch == Watch::Release;
        if (!wanted) {
            return;
        }
        g_latchedPos = Stepper::haltNow(g_probeAxis);
        g_latchedMotor = Stepper::motorPosition(g_probeAxis);
        g_watch = Watch::None;
        g_latched = true;
    }
}
