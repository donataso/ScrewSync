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
#include "ThreadTable.hpp"
#include <math.h>
#include <stdio.h>
#include <string.h>

namespace
{
    using ThreadTable::Entry;
    using ThreadTable::Group;
    using ThreadTable::Table;

    constexpr int32_t DU_PER_UM = 10;
    constexpr int32_t DU_PER_TENTH_MIL = 254; // 0.0001"

    // Common ISO 261 sizes (one display page), in um. Coarse 0 = none.
    struct MetricRow
    {
        int32_t diameterUm;
        int32_t coarseUm;
        int32_t fineUm[3];
    };

    const MetricRow METRIC[] = {
        {1600, 350, {}},
        {2000, 400, {}},
        {2500, 450, {}},
        {3000, 500, {}},
        {4000, 700, {}},
        {5000, 800, {}},
        {6000, 1000, {750}},
        {8000, 1250, {1000}},
        {10000, 1500, {1250, 1000}},
        {12000, 1750, {1500, 1250, 1000}},
        {14000, 2000, {1500}},
        {16000, 2000, {1500}},
        {18000, 2500, {1500}},
        {20000, 2500, {1500}},
        {22000, 2500, {1500}},
        {24000, 3000, {2000, 1500}},
        {27000, 3000, {2000}},
        {30000, 3500, {2000}},
        {36000, 4000, {}},
    };

    // ASME B1.1. Major in 0.0001", TPI x100, 0 = none.
    struct UnifiedRow
    {
        const char *size;
        int32_t major;
        int16_t tpi[3]; // UNC, UNF, UNEF
    };

    const UnifiedRow UNIFIED[] = {
        {"#0", 600, {0, 8000, 0}},
        {"#1", 730, {6400, 7200, 0}},
        {"#2", 860, {5600, 6400, 0}},
        {"#3", 990, {4800, 5600, 0}},
        {"#4", 1120, {4000, 4800, 0}},
        {"#5", 1250, {4000, 4400, 0}},
        {"#6", 1380, {3200, 4000, 0}},
        {"#8", 1640, {3200, 3600, 0}},
        {"#10", 1900, {2400, 3200, 0}},
        {"#12", 2160, {2400, 2800, 3200}},
        {"1/4", 2500, {2000, 2800, 3200}},
        {"5/16", 3125, {1800, 2400, 3200}},
        {"3/8", 3750, {1600, 2400, 3200}},
        {"7/16", 4375, {1400, 2000, 2800}},
        {"1/2", 5000, {1300, 2000, 2800}},
        {"9/16", 5625, {1200, 1800, 2400}},
        {"5/8", 6250, {1100, 1800, 2400}},
        {"11/16", 6875, {0, 0, 2400}},
        {"3/4", 7500, {1000, 1600, 2000}},
        {"13/16", 8125, {0, 0, 2000}},
        {"7/8", 8750, {900, 1400, 2000}},
        {"15/16", 9375, {0, 0, 2000}},
        {"1", 10000, {800, 1200, 2000}},
        {"1-1/16", 10625, {0, 0, 1800}},
        {"1-1/8", 11250, {700, 1200, 1800}},
        {"1-1/4", 12500, {700, 1200, 1800}},
        {"1-5/16", 13125, {0, 0, 1800}},
        {"1-3/8", 13750, {600, 1200, 1800}},
        {"1-7/16", 14375, {0, 0, 1800}},
        {"1-1/2", 15000, {600, 1200, 1800}},
        {"1-9/16", 15625, {0, 0, 1800}},
        {"1-5/8", 16250, {0, 0, 1800}},
        {"1-11/16", 16875, {0, 0, 1800}},
        {"1-3/4", 17500, {500, 0, 0}},
        {"2", 20000, {450, 0, 0}},
        {"2-1/4", 22500, {450, 0, 0}},
        {"2-1/2", 25000, {400, 0, 0}},
        {"2-3/4", 27500, {400, 0, 0}},
        {"3", 30000, {400, 0, 0}},
        {"3-1/4", 32500, {400, 0, 0}},
        {"3-1/2", 35000, {400, 0, 0}},
        {"3-3/4", 37500, {400, 0, 0}},
        {"4", 40000, {400, 0, 0}},
    };

    // BS 84. Major in um, TPI x100, 0 = none.
    struct WhitworthRow
    {
        const char *size;
        int32_t majorUm;
        int16_t tpi[2]; // BSW, BSF
    };

    const WhitworthRow WHITWORTH_ROWS[] = {
        {"1/16", 1588, {6000, 0}},
        {"3/32", 2381, {4800, 0}},
        {"1/8", 3175, {4000, 0}},
        {"5/32", 3969, {3200, 0}},
        {"3/16", 4763, {2400, 3200}},
        {"7/32", 5556, {2400, 2800}},
        {"1/4", 6350, {2000, 2600}},
        {"9/32", 7144, {0, 2600}},
        {"5/16", 7938, {1800, 2200}},
        {"3/8", 9525, {1600, 2000}},
        {"7/16", 11113, {1400, 1800}},
        {"1/2", 12700, {1200, 1600}},
        {"9/16", 14288, {1200, 1600}},
        {"5/8", 15875, {1100, 1400}},
        {"11/16", 17463, {0, 1400}},
        {"3/4", 19050, {1000, 1200}},
        {"7/8", 22225, {900, 1100}},
        {"1", 25400, {800, 1000}},
        {"1-1/8", 28575, {700, 900}},
        {"1-1/4", 31750, {700, 900}},
        {"1-3/8", 34925, {600, 800}},
        {"1-1/2", 38100, {600, 800}},
        {"1-5/8", 41275, {500, 800}},
        {"1-3/4", 44450, {500, 700}},
        {"1-7/8", 47625, {450, 0}},
        {"2", 50800, {450, 700}},
        {"2-1/4", 57150, {400, 600}},
        {"2-1/2", 63500, {400, 600}},
        {"2-3/4", 69850, {350, 600}},
        {"3", 76200, {350, 500}},
        {"3-1/4", 82550, {325, 500}},
        {"3-1/2", 88900, {325, 450}},
        {"3-3/4", 95250, {300, 450}},
        {"4", 101600, {300, 450}},
        {"4-1/4", 107950, {0, 400}},
    };

    // Major in um, TPI x100.
    struct PipeRow
    {
        const char *size;
        int32_t majorUm;
        int16_t tpi;
    };

    // BS EN ISO 228 (G).
    const PipeRow BSP[] = {
        {"1/16", 7722, 2800},
        {"1/8", 9728, 2800},
        {"1/4", 13157, 1900},
        {"3/8", 16662, 1900},
        {"1/2", 20955, 1400},
        {"5/8", 22911, 1400},
        {"3/4", 26441, 1400},
        {"7/8", 30201, 1400},
        {"1", 33249, 1100},
        {"1-1/8", 37897, 1100},
        {"1-1/4", 41910, 1100},
        {"1-3/8", 44323, 1100},
        {"1-1/2", 47803, 1100},
        {"1-5/8", 52883, 1100},
        {"1-3/4", 53746, 1100},
        {"1-7/8", 56998, 1100},
        {"2", 59614, 1100},
        {"2-1/4", 65710, 1100},
        {"2-1/2", 75184, 1100},
        {"2-3/4", 81534, 1100},
        {"3", 87884, 1100},
        {"3-1/2", 100330, 1100},
        {"4", 113030, 1100},
        {"4-1/2", 125730, 1100},
        {"5", 138430, 1100},
        {"5-1/2", 151130, 1100},
        {"6", 163830, 1100},
    };

    // ASME B1.20.1.
    const PipeRow NPT[] = {
        {"1/16", 7895, 2700},
        {"1/8", 10242, 2700},
        {"1/4", 13616, 1800},
        {"3/8", 17055, 1800},
        {"1/2", 21223, 1400},
        {"3/4", 26568, 1400},
        {"1", 33228, 1150},
        {"1-1/4", 41985, 1150},
        {"1-1/2", 48054, 1150},
        {"2", 60092, 1150},
        {"2-1/2", 72699, 800},
        {"3", 88608, 800},
        {"3-1/2", 101316, 800},
        {"4", 113973, 800},
        {"5", 141300, 800},
        {"6", 168275, 800},
        {"8", 219075, 800},
        {"10", 273050, 800},
        {"12", 323850, 800},
    };

    int32_t tpiToPitchDu(int16_t tpiX100)
    {
        return (int32_t)lround(254000.0 * 100.0 / tpiX100);
    }

    // Value in 1/scale units with trailing zeros dropped: 1.6, 10, 0.35.
    void formatScaled(int32_t value, int32_t scale, char *out, size_t outMax)
    {
        if (value % scale == 0) {
            snprintf(out, outMax, "%ld", (long)(value / scale));
            return;
        }
        int32_t frac = value % scale;
        int digits = 0;
        for (int32_t s = scale; s > 1; s /= 10) {
            digits++;
        }
        while (frac % 10 == 0) {
            frac /= 10;
            digits--;
        }
        snprintf(out, outMax, "%ld.%0*ld", (long)(value / scale), digits, (long)frac);
    }

    constexpr float ISO_EXTERNAL = 0.61343f;
    constexpr float ISO_INTERNAL = 0.54127f;
    constexpr float WHITWORTH = 0.640327f;
    constexpr float NPT_DEPTH = 0.8f;
    constexpr float NPT_TAPER = 1.0f / 16.0f;
    constexpr float ANGLE_60 = 60;
    constexpr float ANGLE_WHITWORTH = 55;

    uint8_t addGroup(Table &t, const char *name, float external, float internal, float taper, float flankAngle)
    {
        Group &g = t.groups[t.groupCount];
        snprintf(g.name, sizeof(g.name), "%s", name);
        g.depthExternal = external;
        g.depthInternal = internal;
        g.taper = taper;
        g.flankAngle = flankAngle;
        return (uint8_t)t.groupCount++;
    }

    void addEntry(Table &t, uint8_t group, const char *designation, int32_t majorDu, int32_t pitchDu)
    {
        Entry &e = t.entries[t.entryCount++];
        e = {};
        e.group = group;
        snprintf(e.designation, sizeof(e.designation), "%s", designation);
        e.majorDu = majorDu;
        e.pitchDu = pitchDu;
    }

    // Same bound as the group depth factors.
    bool validDepth(int32_t depthDu, int32_t pitchDu)
    {
        return depthDu >= 0 && depthDu < 2 * (int64_t)pitchDu;
    }

    bool hasNul(const char *s, size_t max)
    {
        return memchr(s, '\0', max) != nullptr;
    }
}

namespace ThreadTable
{
    size_t loadDefaults(Table &t)
    {
        t.groupCount = 0;
        t.entryCount = 0;
        size_t skipped = 0;
        char d[DESIGNATION_MAX];

        uint8_t metric = addGroup(t, "metric", ISO_EXTERNAL, ISO_INTERNAL, 0, ANGLE_60);
        for (const MetricRow &r : METRIC) {
            char dia[12];
            formatScaled(r.diameterUm, 1000, dia, sizeof(dia));
            if (r.coarseUm) {
                snprintf(d, sizeof(d), "M%s", dia);
                addEntry(t, metric, d, r.diameterUm * DU_PER_UM, r.coarseUm * DU_PER_UM);
            }
            for (int32_t pitch : r.fineUm) {
                if (!pitch) {
                    break;
                }
                char p[12];
                formatScaled(pitch, 1000, p, sizeof(p));
                if (snprintf(d, sizeof(d), "M%sx%s", dia, p) < (int)sizeof(d)) {
                    addEntry(t, metric, d, r.diameterUm * DU_PER_UM, pitch * DU_PER_UM);
                } else {
                    skipped++;
                }
            }
        }

        static const char *const UNIFIED_NAMES[] = {"unc", "unf", "unef"};
        static const char *const UNIFIED_SUFFIXES[] = {"UNC", "UNF", "UNEF"};
        for (size_t i = 0; i < 3; i++) {
            uint8_t g = addGroup(t, UNIFIED_NAMES[i], ISO_EXTERNAL, ISO_INTERNAL, 0, ANGLE_60);
            for (const UnifiedRow &r : UNIFIED) {
                if (!r.tpi[i]) {
                    continue;
                }
                char tpi[8];
                formatScaled(r.tpi[i], 100, tpi, sizeof(tpi));
                snprintf(d, sizeof(d), "%s-%s%s", r.size, tpi, UNIFIED_SUFFIXES[i]);
                addEntry(t, g, d, r.major * DU_PER_TENTH_MIL, tpiToPitchDu(r.tpi[i]));
            }
        }

        static const char *const WHITWORTH_NAMES[] = {"bsw", "bsf"};
        static const char *const WHITWORTH_SUFFIXES[] = {"BSW", "BSF"};
        for (size_t i = 0; i < 2; i++) {
            uint8_t g = addGroup(t, WHITWORTH_NAMES[i], WHITWORTH, WHITWORTH, 0, ANGLE_WHITWORTH);
            for (const WhitworthRow &r : WHITWORTH_ROWS) {
                if (!r.tpi[i]) {
                    continue;
                }
                snprintf(d, sizeof(d), "%s%s", r.size, WHITWORTH_SUFFIXES[i]);
                addEntry(t, g, d, r.majorUm * DU_PER_UM, tpiToPitchDu(r.tpi[i]));
            }
        }

        uint8_t bsp = addGroup(t, "bsp", WHITWORTH, WHITWORTH, 0, ANGLE_WHITWORTH);
        for (const PipeRow &r : BSP) {
            snprintf(d, sizeof(d), "G%s", r.size);
            addEntry(t, bsp, d, r.majorUm * DU_PER_UM, tpiToPitchDu(r.tpi));
        }

        uint8_t npt = addGroup(t, "npt", NPT_DEPTH, NPT_DEPTH, NPT_TAPER, ANGLE_60);
        for (const PipeRow &r : NPT) {
            snprintf(d, sizeof(d), "%sNPT", r.size);
            addEntry(t, npt, d, r.majorUm * DU_PER_UM, tpiToPitchDu(r.tpi));
        }

        addGroup(t, "custom", ISO_EXTERNAL, ISO_INTERNAL, 0, ANGLE_60);
        return skipped;
    }

    int findGroup(const Table &t, const char *name)
    {
        for (size_t i = 0; i < t.groupCount; i++) {
            if (!strcmp(t.groups[i].name, name)) {
                return (int)i;
            }
        }
        return -1;
    }

    const Entry *find(const Table &t, const char *group, const char *designation)
    {
        int g = findGroup(t, group);
        if (g < 0) {
            return nullptr;
        }
        for (size_t i = 0; i < t.entryCount; i++) {
            const Entry &e = t.entries[i];
            if (e.group == g && !strcmp(e.designation, designation)) {
                return &e;
            }
        }
        return nullptr;
    }

    int32_t depthDu(const Group &g, int32_t pitchDu, bool internal)
    {
        double factor = internal ? g.depthInternal : g.depthExternal;
        return (int32_t)lround(factor * (pitchDu < 0 ? -pitchDu : pitchDu));
    }

    int32_t depthDu(const Table &t, const Entry &e, bool internal)
    {
        int32_t own = internal ? e.depthInternalDu : e.depthExternalDu;
        return own ? own : depthDu(t.groups[e.group], e.pitchDu, internal);
    }

    float taper(const Table &t, const Entry &e)
    {
        return e.hasTaper ? e.taper : t.groups[e.group].taper;
    }

    float flankAngle(const Table &t, const Entry &e)
    {
        return t.groups[e.group].flankAngle;
    }

    const char *validate(const Table &t)
    {
        if (t.groupCount > MAX_GROUPS || t.entryCount > MAX_ENTRIES) {
            return "too many entries";
        }
        for (size_t i = 0; i < t.groupCount; i++) {
            const Group &g = t.groups[i];
            if (!hasNul(g.name, sizeof(g.name)) || !g.name[0] || strpbrk(g.name, " ,")) {
                return "bad group name";
            }
            if (findGroup(t, g.name) != (int)i) {
                return "duplicate group";
            }
            if (!(g.depthExternal > 0 && g.depthExternal < 2 && g.depthInternal > 0 && g.depthInternal < 2)) {
                return "bad depth factor";
            }
            if (!(g.taper > -1 && g.taper < 1)) {
                return "bad taper";
            }
            if (!(g.flankAngle > 0 && g.flankAngle < MAX_FLANK_ANGLE)) {
                return "bad flank angle";
            }
            size_t entries = 0;
            for (size_t k = 0; k < t.entryCount; k++) {
                entries += t.entries[k].group == i;
            }
            if (entries > MAX_GROUP_ENTRIES) {
                return "too many entries in a group";
            }
        }
        for (size_t i = 0; i < t.entryCount; i++) {
            const Entry &e = t.entries[i];
            if (e.group >= t.groupCount) {
                return "entry without group";
            }
            if (!hasNul(e.designation, sizeof(e.designation)) || !e.designation[0] || strpbrk(e.designation, " ,")) {
                return "bad designation";
            }
            if (e.pitchDu <= 0 || e.majorDu < 0) {
                return "bad pitch or diameter";
            }
            if (!validDepth(e.depthExternalDu, e.pitchDu) || !validDepth(e.depthInternalDu, e.pitchDu)) {
                return "bad depth";
            }
            if (e.hasTaper && !(e.taper > -1 && e.taper < 1)) {
                return "bad taper";
            }
            if (find(t, t.groups[e.group].name, e.designation) != &e) {
                return "duplicate designation";
            }
        }
        return nullptr;
    }
}
