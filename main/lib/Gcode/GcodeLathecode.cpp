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
#include "GcodeInterpreter.hpp"

namespace Gcode
{
    void Lathecode::begin(Interpreter &interp, int32_t startZDu, int32_t startXDu)
    {
        interp.begin(startZDu, startXDu, Interpreter::Motion::Feed);
        thread_ = false;
        leadSet_ = false;
        leadDu_ = 0;
        unitScaleDu_ = DU_PER_MM;
    }

    const char *Lathecode::translate(Words &words)
    {
        bool hasAxis = false;
        Word *f = nullptr;
        for (size_t i = 0; i < words.count; i++) {
            Word &w = words.words[i];
            switch (w.letter) {
            case 'G':
                if (w.value == 32) {
                    thread_ = true;
                    w.value = 33;
                } else if (w.value == 33) {
                    return "unsupported command";
                } else if (w.value == 0 || w.value == 1) {
                    thread_ = false;
                } else if (w.value == 20) {
                    unitScaleDu_ = DU_PER_INCH;
                } else if (w.value == 21) {
                    unitScaleDu_ = DU_PER_MM;
                }
                break;
            case 'K':
                return "unsupported axis";
            case 'F':
                if (f) {
                    return "duplicate word";
                }
                f = &w;
                break;
            case 'Z':
            case 'X':
                hasAxis = true;
                break;
            default:
                break;
            }
        }
        if (!thread_) {
            return nullptr;
        }
        if (f) {
            f->letter = 'K';
            leadDu_ = f->value * unitScaleDu_;
            leadSet_ = true;
        } else if (hasAxis && leadSet_) {
            if (words.count >= MAX_WORDS) {
                return "too many words";
            }
            words.words[words.count++] = {'K', leadDu_ / unitScaleDu_};
        }
        return nullptr;
    }
}
