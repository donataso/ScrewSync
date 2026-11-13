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
#ifndef ModePassPlan_h
#define ModePassPlan_h

#include <stdint.h>

// Depth levels and starts of a multi-pass job. Level 0 = not started,
// levels 1..levels step deeper, later ones are finish passes at full depth.
// Each level runs once per start.
namespace Mode::Detail
{
    struct PassPlan
    {
        int32_t levels;
        int32_t finishLevels;
        int32_t starts;
        bool squaredDepth; // deeper first passes
        double depthDu;
        double lengthDu;
        double endReductionDu; // shortens each successive level
        double flankTan;       // alternating flank infeed: main shift per depth, 0 = radial
    };

    struct PassCursor
    {
        int32_t level;
        int32_t startIndex;
        int32_t skipLevels; // applied when the next depth level starts
    };

    int32_t passTotalLevels(const PassPlan &plan);
    double passDepthFraction(const PassPlan &plan, int32_t level);
    double passDepthDu(const PassPlan &plan, int32_t level);
    // Main-axis shift of a level's start, in the cut direction: each level
    // slides along the flank the previous one cut, alternating sides.
    double passFlankOffsetDu(const PassPlan &plan, int32_t level);
    double passLengthDu(const PassPlan &plan, int32_t level);

    // The level of the pass after the cursor's.
    int32_t passNextLevel(const PassPlan &plan, const PassCursor &cursor);
    // After a pass: next start, else next depth level. False when done.
    bool passAdvance(const PassPlan &plan, PassCursor &cursor);
    void passSkipLevel(const PassPlan &plan, PassCursor &cursor);

    int32_t passCount(const PassPlan &plan);
    // 1-based, 0 before the first pass.
    int32_t passNumber(const PassPlan &plan, const PassCursor &cursor);
}

#endif
