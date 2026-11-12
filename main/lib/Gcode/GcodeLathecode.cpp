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
#include "GcodeLathecode.hpp"
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

    // G/M codes must be exact integers; no rounding.
    bool exactInt(double value, int &outInt)
    {
        if (!(value >= 0 && value <= 1000)) {
            return false;
        }
        int truncated = (int)value;
        if ((double)truncated != value) {
            return false;
        }
        outInt = truncated;
        return true;
    }
}

namespace Gcode
{
    void Lathecode::begin(int32_t startZDu, int32_t startXDu)
    {
        motion_ = Motion::Feed;
        absolute_ = true;
        unitScaleDu_ = DU_PER_MM;
        feedDuPerMin_ = 0;
        feedSet_ = false;
        threadLeadDu_ = 0;
        threadLeadSet_ = false;
        zDu_ = startZDu;
        xDu_ = startXDu;
    }

    const char *Lathecode::line(const Words &words, uint32_t lineNo, StepSink &sink)
    {
        bool hasF = false, hasZ = false, hasX = false, hasM = false, hasG32 = false;
        double fVal = 0, zVal = 0, xVal = 0;
        StepKind mKind = StepKind::Pause;

        for (size_t i = 0; i < words.count; i++) {
            const Word &w = words.words[i];
            switch (w.letter) {
            case 'G':
            {
                int g;
                if (!exactInt(w.value, g)) {
                    return "unsupported command";
                }
                switch (g) {
                case 0:
                    motion_ = Motion::Rapid;
                    break;
                case 1:
                    motion_ = Motion::Feed;
                    break;
                case 32:
                    motion_ = Motion::Thread;
                    hasG32 = true;
                    break;
                case 20:
                    unitScaleDu_ = DU_PER_INCH;
                    break;
                case 21:
                    unitScaleDu_ = DU_PER_MM;
                    break;
                case 90:
                    absolute_ = true;
                    break;
                case 91:
                    absolute_ = false;
                    break;
                case 94:
                case 18:
                    break;
                default:
                    return "unsupported command";
                }
                break;
            }
            case 'M':
            {
                int m;
                if (!exactInt(w.value, m)) {
                    return "unsupported command";
                }
                switch (m) {
                case 0:
                    mKind = StepKind::Pause;
                    break;
                case 1:
                    mKind = StepKind::OptionalPause;
                    break;
                case 2:
                case 30:
                    mKind = StepKind::End;
                    break;
                default:
                    return "unsupported command";
                }
                hasM = true;
                break;
            }
            case 'T':
                break;
            case 'F':
                if (hasF) {
                    return "duplicate word";
                }
                hasF = true;
                fVal = w.value;
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

        if (hasG32 && !hasZ && !hasX) {
            return "G32 requires axis move";
        }

        // G32's F is the thread lead for this move, not the modal feed.
        bool threadMove = (motion_ == Motion::Thread);
        if (hasF) {
            if (fVal <= 0) {
                return "value out of range";
            }
            int32_t fDu;
            if (!scaledValueToDu(fVal, unitScaleDu_, fDu) || fDu <= 0) {
                return "value out of range";
            }
            if (threadMove) {
                threadLeadDu_ = fDu;
                threadLeadSet_ = true;
            } else {
                feedDuPerMin_ = fDu;
                feedSet_ = true;
            }
        }

        if (hasZ || hasX) {
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
                    if (!threadLeadSet_) {
                        return "no feed rate";
                    }
                    step.kind = StepKind::Thread;
                    step.feed = threadLeadDu_;
                }
                zDu_ = newZDu;
                xDu_ = newXDu;
                sink.emit(step);
            } else if (threadMove) {
                return "G32 requires axis move";
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
