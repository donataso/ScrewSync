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
#include "RunSettings.hpp"
#include "PersistBlob.hpp"
#include "JogStep.hpp"
#include "config.hpp"
#include <string.h>
#include <zephyr/kernel.h>
#include <initializer_list>

namespace
{
    constexpr const char *NVS_NS = "state";
    constexpr size_t MAX_MODE_PARAMS_SIZE = 128;

    K_MUTEX_DEFINE(g_lock);
    RunSettings::Units g_units = RunSettings::Units::Mm;
    int32_t g_jogStepDu = DEFAULT_JOG_STEP_DU;
    int32_t g_customJogStepDu = 0;
    Mode::Id g_mode = Mode::Id::Gear;

    const char *disabledKey(Stepper::Axis axis)
    {
        return axis == Stepper::Axis::Z ? "disabledZ" : "disabledX";
    }

    template <typename T>
    void loadValue(const char *key, T &value)
    {
        T v;
        if (PersistBlob::load(NVS_NS, key, &v, sizeof(v)) == sizeof(v)) {
            value = v;
        }
    }

    template <typename T>
    void store(const char *key, T &field, const T &value)
    {
        k_mutex_lock(&g_lock, K_FOREVER);
        field = value;
        k_mutex_unlock(&g_lock);
        PersistBlob::save(NVS_NS, key, &value, sizeof(value));
    }

    template <typename T>
    T read(const T &field)
    {
        k_mutex_lock(&g_lock, K_FOREVER);
        T v = field;
        k_mutex_unlock(&g_lock);
        return v;
    }
}

namespace RunSettings
{
    void load()
    {
        for (Stepper::Axis axis : {Stepper::Axis::Z, Stepper::Axis::X}) {
            bool disabled = false;
            loadValue(disabledKey(axis), disabled);
            Stepper::setDisabled(axis, disabled);
        }

        loadValue("units", g_units);
        loadValue("jogStep", g_jogStepDu);
        if (!jogStepValid(g_jogStepDu)) {
            g_jogStepDu = DEFAULT_JOG_STEP_DU;
        }
        loadValue("jogCustom", g_customJogStepDu);
        if (!jogStepValid(g_customJogStepDu)) {
            g_customJogStepDu = 0;
        }
        loadValue("mode", g_mode);
        if ((size_t)g_mode >= Mode::COUNT) {
            g_mode = Mode::Id::Gear;
        }
    }

    void setAxisDisabled(Stepper::Axis axis, bool disabled)
    {
        Stepper::setDisabled(axis, disabled);
        PersistBlob::save(NVS_NS, disabledKey(axis), &disabled, sizeof(disabled));
    }

    Units units()
    {
        return read(g_units);
    }
    const char *unitsName()
    {
        return units() == Units::Inch ? "inch" : "mm";
    }
    void setUnits(Units units)
    {
        store("units", g_units, units);
    }

    bool parseUnits(const char *name, Units &out)
    {
        if (!strcmp(name, "mm")) {
            out = Units::Mm;
        } else if (!strcmp(name, "inch")) {
            out = Units::Inch;
        } else {
            return false;
        }
        return true;
    }

    int32_t jogStepDu()
    {
        return read(g_jogStepDu);
    }
    bool jogStepValid(int32_t du)
    {
        return du > 0 && du <= Jog::MAX_STEP_DU;
    }

    bool setJogStepDu(int32_t du, bool persist)
    {
        if (!jogStepValid(du)) {
            return false;
        }
        bool custom = !Jog::isStepPreset(du, units() == Units::Inch);
        k_mutex_lock(&g_lock, K_FOREVER);
        g_jogStepDu = du;
        if (custom) {
            g_customJogStepDu = du;
        }
        k_mutex_unlock(&g_lock);
        if (persist) {
            persistJogStep();
        }
        return true;
    }

    void persistJogStep()
    {
        int32_t du = read(g_jogStepDu);
        int32_t custom = read(g_customJogStepDu);
        PersistBlob::save(NVS_NS, "jogStep", &du, sizeof(du));
        PersistBlob::save(NVS_NS, "jogCustom", &custom, sizeof(custom));
    }

    void nextJogStep()
    {
        setJogStepDu(Jog::nextStep(jogStepDu(), read(g_customJogStepDu), units() == Units::Inch));
    }

    Mode::Id mode()
    {
        return read(g_mode);
    }
    void setMode(Mode::Id mode)
    {
        store("mode", g_mode, mode);
    }

    bool loadModeParams(const char *key, void *out, size_t size)
    {
        uint8_t buf[MAX_MODE_PARAMS_SIZE + 1];
        if (size > MAX_MODE_PARAMS_SIZE || PersistBlob::load(NVS_NS, key, buf, sizeof(buf)) != size) {
            return false;
        }
        memcpy(out, buf, size);
        return true;
    }

    void saveModeParams(const char *key, const void *params, size_t size)
    {
        PersistBlob::save(NVS_NS, key, params, size);
    }
}
