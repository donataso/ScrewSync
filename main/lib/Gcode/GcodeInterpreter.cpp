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
#include "GcodeInterpreter.hpp"
#include <math.h>

namespace
{
    using namespace Gcode;

    // Converts a word's raw value (in current units) to du, rejecting
    // anything that would overflow int32_t before rounding -- llround on
    // an out-of-range double is undefined behaviour.
    bool scaledValueToDu(double value, int32_t unitScaleDu, int32_t &outDu)
    {
        double scaled = value * (double)unitScaleDu;
        if (!isfinite(scaled) || scaled > (double)INT32_MAX || scaled < (double)INT32_MIN) {
            return false;
        }
        outDu = (int32_t)llround(scaled);
        return true;
    }

    bool axisTargetDu(double value, int32_t currentDu, bool absolute, int32_t unitScaleDu, int32_t &outDu)
    {
        int32_t rawDu;
        if (!scaledValueToDu(value, unitScaleDu, rawDu)) {
            return false;
        }
        int64_t target = absolute ? (int64_t)rawDu : (int64_t)currentDu + (int64_t)rawDu;
        if (target < INT32_MIN || target > INT32_MAX) {
            return false;
        }
        outDu = (int32_t)target;
        return true;
    }

    // G/M numbers in tenths (G38.2 = 382); anything finer is refused.
    bool exactTenths(double value, int &outTenths)
    {
        if (!(value >= 0 && value <= 1000)) {
            return false;
        }
        double tenths = value * 10;
        int rounded = (int)llround(tenths);
        if (fabs(tenths - rounded) > 1e-6) {
            return false;
        }
        outTenths = rounded;
        return true;
    }
}

namespace Gcode
{
    void Interpreter::begin(int32_t startZDu, int32_t startXDu, Motion motion)
    {
        motion_ = motion;
        absolute_ = true;
        unitScaleDu_ = DU_PER_MM;
        feedDuPerMin_ = 0;
        feedSet_ = false;
        zDu_ = startZDu;
        xDu_ = startXDu;
    }

    const char *Interpreter::line(const Words &words, uint32_t lineNo, StepSink &sink)
    {
        bool hasF = false, hasK = false, hasZ = false, hasX = false, hasM = false, hasG33 = false;
        double fVal = 0, kVal = 0, zVal = 0, xVal = 0;
        StepKind mKind = StepKind::Pause;

        for (size_t i = 0; i < words.count; i++) {
            const Word &w = words.words[i];
            int code;
            switch (w.letter) {
            case 'G':
                if (!exactTenths(w.value, code)) {
                    return "unsupported command";
                }
                switch (code) {
                case 0:
                    motion_ = Motion::Rapid;
                    break;
                case 10:
                    motion_ = Motion::Feed;
                    break;
                case 330:
                    motion_ = Motion::Thread;
                    hasG33 = true;
                    break;
                case 200:
                    unitScaleDu_ = DU_PER_INCH;
                    break;
                case 210:
                    unitScaleDu_ = DU_PER_MM;
                    break;
                case 900:
                    absolute_ = true;
                    break;
                case 910:
                    absolute_ = false;
                    break;
                case 940:
                case 180:
                    break;
                default:
                    return "unsupported command";
                }
                break;
            case 'M':
                if (!exactTenths(w.value, code)) {
                    return "unsupported command";
                }
                switch (code) {
                case 0:
                    mKind = StepKind::Pause;
                    break;
                case 10:
                    mKind = StepKind::OptionalPause;
                    break;
                case 20:
                case 300:
                    mKind = StepKind::End;
                    break;
                default:
                    return "unsupported command";
                }
                hasM = true;
                break;
            case 'T':
                break;
            case 'F':
                if (hasF) {
                    return "duplicate word";
                }
                hasF = true;
                fVal = w.value;
                break;
            case 'K':
                if (hasK) {
                    return "duplicate word";
                }
                hasK = true;
                kVal = w.value;
                break;
            case 'Z':
                if (hasZ) {
                    return "duplicate word";
                }
                hasZ = true;
                zVal = w.value;
                break;
            case 'X':
                if (hasX) {
                    return "duplicate word";
                }
                hasX = true;
                xVal = w.value;
                break;
            default:
                return "unsupported axis";
            }
        }

        if (hasG33 && !hasZ && !hasX) {
            return "G33 requires axis move";
        }
        if (hasK && motion_ != Motion::Thread) {
            return "K requires G33";
        }

        if (hasF) {
            int32_t fDu;
            if (!(fVal > 0) || !scaledValueToDu(fVal, unitScaleDu_, fDu) || fDu <= 0) {
                return "value out of range";
            }
            feedDuPerMin_ = fDu;
            feedSet_ = true;
        }

        int32_t leadDu = 0;
        if (hasK && (!(kVal > 0) || !scaledValueToDu(kVal, unitScaleDu_, leadDu) || leadDu <= 0)) {
            return "value out of range";
        }

        if (hasZ || hasX) {
            if (motion_ == Motion::None) {
                return "no motion mode";
            }
            int32_t newZDu = zDu_, newXDu = xDu_;
            if (hasZ && !axisTargetDu(zVal, zDu_, absolute_, unitScaleDu_, newZDu)) {
                return "value out of range";
            }
            if (hasX && !axisTargetDu(xVal, xDu_, absolute_, unitScaleDu_, newXDu)) {
                return "value out of range";
            }

            if (newZDu != zDu_ || newXDu != xDu_) {
                Step step{};
                step.line = lineNo;
                step.zDu = newZDu;
                step.xDu = newXDu;
                if (motion_ == Motion::Rapid) {
                    step.kind = StepKind::Rapid;
                    step.feed = 0;
                } else if (motion_ == Motion::Feed) {
                    if (!feedSet_) {
                        return "no feed rate";
                    }
                    step.kind = StepKind::Feed;
                    step.feed = feedDuPerMin_;
                } else {
                    if (!hasK) {
                        return "G33 requires K";
                    }
                    step.kind = StepKind::Thread;
                    step.feed = leadDu;
                }
                zDu_ = newZDu;
                xDu_ = newXDu;
                sink.emit(step);
            } else if (motion_ == Motion::Thread) {
                return "G33 requires axis move";
            }
        }

        if (hasM) {
            Step step{};
            step.line = lineNo;
            step.zDu = zDu_;
            step.xDu = xDu_;
            step.feed = 0;
            step.kind = mKind;
            sink.emit(step);
        }

        return nullptr;
    }
}
