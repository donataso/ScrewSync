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
#ifndef ModeMdiQueue_h
#define ModeMdiQueue_h

#include "GcodeCore.hpp"
#include "GcodeNgc.hpp"
#include <stddef.h>
#include <stdint.h>
#include <string.h>

namespace Mode::Detail
{
    class MdiQueue
    {
    public:
        static constexpr size_t LEN = 4;
        static constexpr size_t TEXT_MAX = Gcode::MESSAGE_MAX * 2;

        size_t count() const
        {
            return count_;
        }

        uint32_t completed() const
        {
            return completed_;
        }

        size_t free() const
        {
            return LEN - count_;
        }

        const Gcode::Step &head() const
        {
            return steps_[head_];
        }

        const Gcode::Step &at(size_t index) const
        {
            return steps_[(head_ + index) % LEN];
        }

        const char *headText() const
        {
            return texts_[textHead_];
        }

        void push(const Gcode::Step &step)
        {
            steps_[(head_ + count_) % LEN] = step;
            count_++;
        }

        void pushPrint(const char *text)
        {
            Gcode::Step step = {};
            step.kind = Gcode::StepKind::Print;
            strncpy(texts_[(textHead_ + textCount_) % LEN], text, TEXT_MAX - 1);
            texts_[(textHead_ + textCount_) % LEN][TEXT_MAX - 1] = '\0';
            textCount_++;
            push(step);
        }

        void pop()
        {
            if (head().kind == Gcode::StepKind::Print) {
                textHead_ = (textHead_ + 1) % LEN;
                textCount_--;
            }
            head_ = (head_ + 1) % LEN;
            count_--;
            completed_++;
        }

        void clear()
        {
            head_ = 0;
            count_ = 0;
            textHead_ = 0;
            textCount_ = 0;
        }

    private:
        Gcode::Step steps_[LEN];
        char texts_[LEN][TEXT_MAX];
        size_t head_ = 0;
        size_t count_ = 0;
        size_t textHead_ = 0;
        size_t textCount_ = 0;
        uint32_t completed_ = 0;
    };
}

#endif
