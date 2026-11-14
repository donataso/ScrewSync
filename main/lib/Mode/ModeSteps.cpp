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
#include "Motion.hpp"
#include "Rpm.hpp"
#include "Wcs.hpp"
#include <math.h>
#include <stdlib.h>
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
        return duToSteps(axis, (double)Wcs::toMachine(axis, axis == Stepper::Axis::Z ? s.zDu : s.xDu));
    }

    const char *clampReason(Stepper::Axis axis)
    {
        return Stepper::isDisabled(axis) ? "axis disabled" : "travel limit";
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
        return kind == Gcode::StepKind::Rapid || kind == Gcode::StepKind::Feed || kind == Gcode::StepKind::Thread;
    }

    void applyMachineStep(const Gcode::Step &s)
    {
        if (s.kind == Gcode::StepKind::SpindleAngle) {
            Motion::setSpindleAngle(s.feed / 1000.0);
            return;
        }
        for (Stepper::Axis axis : {Stepper::Axis::Z, Stepper::Axis::X}) {
            bool isZ = axis == Stepper::Axis::Z;
            if (!(s.axes & (isZ ? Gcode::AXIS_Z : Gcode::AXIS_X))) {
                continue;
            }
            int32_t value = isZ ? s.zDu : s.xDu;
            if (s.kind == Gcode::StepKind::SetPosition) {
                Wcs::setPosition(axis, value);
            } else if (s.kind == Gcode::StepKind::Motors) {
                Motion::setAxisEnabled(axis, value != 0);
            }
        }
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
    }

    bool StepMotion::drive(const Gcode::Step &s, int64_t spindlePos, bool spindleGated, const char *&error)
    {
        error = nullptr;
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
