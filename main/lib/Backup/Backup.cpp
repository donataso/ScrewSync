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
#include "Backup.hpp"
#include "Machine.hpp"
#include "Mode.hpp"
#include "RunSettings.hpp"
#include "Settings.hpp"
#include "Stepper.hpp"
#include "ThreadStore.hpp"
#include <stdio.h>
#include <string.h>
#include <zephyr/data/json.h>
#include <zephyr/sys/util.h>

#ifdef CONFIG_ESP_SPIRAM
#define EXT_RAM_BSS __attribute__((section(".ext_ram.bss")))
#else
#define EXT_RAM_BSS
#endif

namespace
{
    constexpr size_t PARAMS_MAX = 256;

    struct ModeJson
    {
        const char *mode;
        const char *params;
    };

    struct RunJson
    {
        const char *units;
        int32_t jogStepDu;
        const char *mode;
        bool disabledZ;
        bool disabledX;
        ModeJson modes[Mode::COUNT];
        size_t modeCount;
    };

    struct Doc
    {
        Settings::Conf settings;
        RunJson run;
        ThreadStore::JsonTable threads;
    };

    // Raw storage: Conf's member initializers would make Doc a static constructor in PSRAM.
    alignas(Doc) uint8_t g_docStorage[sizeof(Doc)] EXT_RAM_BSS;
    Doc &g_doc = *reinterpret_cast<Doc *>(g_docStorage);
    ThreadTable::Table g_table EXT_RAM_BSS;
    char g_params[Mode::COUNT][PARAMS_MAX] EXT_RAM_BSS;

    const struct json_obj_descr MODE_DESCR[] = {
        JSON_OBJ_DESCR_PRIM(ModeJson, mode, JSON_TOK_STRING),
        JSON_OBJ_DESCR_PRIM(ModeJson, params, JSON_TOK_STRING),
    };

    const struct json_obj_descr RUN_DESCR[] = {
        JSON_OBJ_DESCR_PRIM(RunJson, units, JSON_TOK_STRING),
        JSON_OBJ_DESCR_PRIM(RunJson, jogStepDu, JSON_TOK_NUMBER),
        JSON_OBJ_DESCR_PRIM(RunJson, mode, JSON_TOK_STRING),
        JSON_OBJ_DESCR_PRIM(RunJson, disabledZ, JSON_TOK_TRUE),
        JSON_OBJ_DESCR_PRIM(RunJson, disabledX, JSON_TOK_TRUE),
        JSON_OBJ_DESCR_OBJ_ARRAY(RunJson, modes, Mode::COUNT, modeCount, MODE_DESCR, ARRAY_SIZE(MODE_DESCR)),
    };

    const struct json_obj_descr DOC_DESCR[] = {
        JSON_OBJ_DESCR_OBJECT(Doc, settings, Settings::CONF_JSON_DESCR),
        JSON_OBJ_DESCR_OBJECT(Doc, run, RUN_DESCR),
        JSON_OBJ_DESCR_OBJECT(Doc, threads, ThreadStore::JSON_TABLE_DESCR),
    };

    void currentRun(RunJson &run)
    {
        run.units = RunSettings::unitsName();
        run.jogStepDu = RunSettings::jogStepDu();
        run.mode = Mode::name(Mode::current());
        run.disabledZ = Stepper::isDisabled(Stepper::Axis::Z);
        run.disabledX = Stepper::isDisabled(Stepper::Axis::X);
        run.modeCount = 0;
    }

    const char *checkRun(const RunJson &run, RunSettings::Units &units, Mode::Id &mode)
    {
        static char reason[64];
        if (!run.units || !RunSettings::parseUnits(run.units, units)) {
            return "run: bad units";
        }
        if (!RunSettings::jogStepValid(run.jogStepDu)) {
            return "run: bad jogStepDu";
        }
        if (!run.mode || !Mode::parse(run.mode, mode)) {
            return "run: bad mode";
        }
        for (size_t i = 0; i < run.modeCount; i++) {
            const ModeJson &m = run.modes[i];
            Mode::Id id;
            if (!m.mode || !m.params || !Mode::parse(m.mode, id)) {
                return "run: bad modes entry";
            }
            const char *refusal = Mode::loadParams(id, m.params, false);
            if (refusal) {
                snprintf(reason, sizeof(reason), "%s: %s", m.mode, refusal);
                return reason;
            }
        }
        return nullptr;
    }

    void applyRun(const RunJson &run, RunSettings::Units units, Mode::Id mode)
    {
        RunSettings::setUnits(units);
        RunSettings::setJogStepDu(run.jogStepDu);
        RunSettings::setAxisDisabled(Stepper::Axis::Z, run.disabledZ);
        RunSettings::setAxisDisabled(Stepper::Axis::X, run.disabledX);
        for (size_t i = 0; i < run.modeCount; i++) {
            Mode::Id id;
            Mode::parse(run.modes[i].mode, id);
            Mode::loadParams(id, run.modes[i].params, true);
        }
        Mode::select(mode);
    }
}

namespace Backup
{
    int toJson(char *out, size_t outMax)
    {
        g_doc.settings = Settings::g_active;
        currentRun(g_doc.run);
        for (size_t i = 0; i < Mode::COUNT; i++) {
            if (Mode::describe((Mode::Id)i, g_params[i], PARAMS_MAX)) {
                g_doc.run.modes[g_doc.run.modeCount++] = {Mode::name((Mode::Id)i), g_params[i]};
            }
        }
        ThreadStore::snapshot(g_table);
        ThreadStore::toJson(g_table, g_doc.threads);
        int rc = json_obj_encode_buf(DOC_DESCR, ARRAY_SIZE(DOC_DESCR), &g_doc, out, outMax);
        return rc == 0 ? (int)strlen(out) : -1;
    }

    const char *fromJson(char *json, size_t len)
    {
        if (Machine::state() != Machine::State::Idle || Mode::running()) {
            return "machine not idle";
        }
        g_doc.settings = Settings::g_active;
        currentRun(g_doc.run);
        memset(&g_doc.threads, 0, sizeof(g_doc.threads));
        int64_t mask = json_obj_parse(json, len, DOC_DESCR, ARRAY_SIZE(DOC_DESCR), &g_doc);
        if (mask <= 0) {
            return "bad json";
        }
        bool hasSettings = mask & BIT(0);
        bool hasRun = mask & BIT(1);
        bool hasThreads = mask & BIT(2);

        static char reason[64];
        if (hasSettings && !Settings::validate(g_doc.settings, reason, sizeof(reason))) {
            return reason;
        }
        RunSettings::Units units;
        Mode::Id mode;
        const char *refusal = hasRun ? checkRun(g_doc.run, units, mode) : nullptr;
        if (refusal) {
            return refusal;
        }
        if (hasThreads) {
            refusal = ThreadStore::fromJson(g_doc.threads, g_table);
            if (refusal) {
                snprintf(reason, sizeof(reason), "threads: %s", refusal);
                return reason;
            }
        }

        if (hasSettings) {
            Settings::replace(g_doc.settings);
        }
        if (hasRun) {
            applyRun(g_doc.run, units, mode);
        }
        if (hasThreads) {
            ThreadStore::replace(g_table);
        }
        return nullptr;
    }
}
