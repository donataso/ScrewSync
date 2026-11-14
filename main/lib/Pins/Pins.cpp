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
#include "Pins.hpp"
#include "PinMapJson.hpp"
#include "Machine.hpp"
#include "Mode.hpp"
#include "PersistBlob.hpp"

#include <dt-bindings/screwsync/pins.h>
#include <esp_rom_gpio.h>
#include <soc/gpio_pins.h>
#include <soc/gpio_sig_map.h>
#include <string.h>
#include <zephyr/devicetree.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(pins, LOG_LEVEL_INF);

#define CONNECTORS_NODE DT_COMPAT_GET_ANY_STATUS_OKAY(screwsync_connectors)

#if !DT_NODE_EXISTS(CONNECTORS_NODE)
#error "The board overlay needs a screwsync,connectors node"
#endif
#if !DT_NODE_EXISTS(DT_NODELABEL(pcnt))
#error "The board needs the pcnt node"
#endif

using namespace PinMap;

namespace
{
    constexpr const char *NVS_NS = "pins";
    constexpr const char *NVS_KEY = "map";
    constexpr uint32_t MAGIC = 0x50494e31; // "PIN1"
    // GPIO matrix input signals of PCNT unit n are 4 apart.
    constexpr uint32_t PCNT_SIGNAL_STRIDE = PCNT_SIG_CH0_IN1_IDX - PCNT_SIG_CH0_IN0_IDX;

    struct Stored
    {
        uint32_t magic;
        uint32_t connectorCount;
        Map map;
    };

    constexpr int16_t pinCode(uint32_t v)
    {
        return v == PIN_5V ? PinMap::POWER_5V : (v == PIN_GND ? PinMap::POWER_GND : (int16_t)v);
    }

#define PIN_CODE(node, prop, idx) pinCode(DT_PROP_BY_IDX(node, prop, idx)),
#define CONNECTOR_INIT(node) \
    {DT_NODE_FULL_NAME(node), DT_PROP(node, title), DT_PROP(node, row), DT_PROP(node, col), DT_PROP_LEN(node, pins), {DT_FOREACH_PROP_ELEM(node, pins, PIN_CODE)}},

    const Connector CONNECTORS[] = {DT_FOREACH_CHILD(CONNECTORS_NODE, CONNECTOR_INIT)};

    // Each list ends with a filler, so that a missing property still gives a valid array.
#define ELEM(node, prop, idx) DT_PROP_BY_IDX(node, prop, idx),
#define LIST(prop) COND_CODE_1(DT_NODE_HAS_PROP(CONNECTORS_NODE, prop), (DT_FOREACH_PROP_ELEM(CONNECTORS_NODE, prop, ELEM)), ())
#define COUNT(prop) DT_PROP_LEN_OR(CONNECTORS_NODE, prop, 0)

    const uint8_t STRAPPING[] = {LIST(strapping_gpios) 0};
    const uint8_t RESERVED[] = {LIST(reserved_gpios) 0};
    const char *const RESERVED_NOTES[] = {LIST(reserved_notes) ""};

    static_assert(ARRAY_SIZE(CONNECTORS) <= MAX_CONNECTORS, "too many connectors");
    static_assert(COUNT(strapping_gpios) <= MAX_STRAPPING, "too many strapping pins");
    static_assert(COUNT(reserved_gpios) <= MAX_RESERVED, "too many reserved pins");
    static_assert(COUNT(reserved_notes) == COUNT(reserved_gpios), "one note per reserved pin");

    const int32_t DEFAULT_Z_MOTOR[] = {LIST(z_motor) - 1};
    const int32_t DEFAULT_X_MOTOR[] = {LIST(x_motor) - 1};
    const int32_t DEFAULT_SPINDLE[] = {LIST(spindle) - 1};
    const int32_t DEFAULT_Z_HANDWHEEL[] = {LIST(z_handwheel) - 1};
    const int32_t DEFAULT_X_HANDWHEEL[] = {LIST(x_handwheel) - 1};
    const int32_t DEFAULT_PROBE[] = {LIST(probe) - 1};

    struct DefaultPins
    {
        Fn fn;
        const int32_t *gpios;
        size_t count;
    };

    const DefaultPins DEFAULTS[] = {
        {Fn::ZMotor, DEFAULT_Z_MOTOR, COUNT(z_motor)},
        {Fn::XMotor, DEFAULT_X_MOTOR, COUNT(x_motor)},
        {Fn::Spindle, DEFAULT_SPINDLE, COUNT(spindle)},
        {Fn::ZHandwheel, DEFAULT_Z_HANDWHEEL, COUNT(z_handwheel)},
        {Fn::XHandwheel, DEFAULT_X_HANDWHEEL, COUNT(x_handwheel)},
        {Fn::Probe, DEFAULT_PROBE, COUNT(probe)},
    };

    const struct device *const GPIO0 = DEVICE_DT_GET(DT_NODELABEL(gpio0));
    const struct device *const GPIO1 = DEVICE_DT_GET(DT_NODELABEL(gpio1));

    Board g_board;
    Map g_active;
    Stored g_stored;
    bool g_defaults = false;
    char g_err[ERROR_MAX];

    void buildBoard()
    {
        g_board = {};
        g_board.name = DT_PROP(CONNECTORS_NODE, board_name);
        g_board.connectorCount = ARRAY_SIZE(CONNECTORS);
        for (size_t i = 0; i < ARRAY_SIZE(CONNECTORS); i++) {
            g_board.connectors[i] = CONNECTORS[i];
        }
        g_board.strappingCount = COUNT(strapping_gpios);
        memcpy(g_board.strapping, STRAPPING, g_board.strappingCount);
        g_board.reservedCount = COUNT(reserved_gpios);
        memcpy(g_board.reserved, RESERVED, g_board.reservedCount);
        for (size_t i = 0; i < g_board.reservedCount; i++) {
            g_board.reservedNotes[i] = RESERVED_NOTES[i];
        }
        g_board.pcntUnits = DT_CHILD_NUM_STATUS_OKAY(DT_NODELABEL(pcnt));
    }

    void defaults(Map &out)
    {
        memset(&out, 0, sizeof(out));
        for (const DefaultPins &d : DEFAULTS) {
            if (d.count && !fromGpios(g_board, d.fn, d.gpios, d.count, out.fn[(size_t)d.fn])) {
                LOG_ERR("default pins of %s are not next to each other in one connector", def(d.fn).id);
            }
        }
    }

    bool loadSaved(Map &out)
    {
        size_t n = PersistBlob::load(NVS_NS, NVS_KEY, &g_stored, sizeof(g_stored));
        if (n == 0) {
            return false;
        }
        if (n != sizeof(g_stored) || g_stored.magic != MAGIC || g_stored.connectorCount != g_board.connectorCount) {
            LOG_ERR("saved pin map does not fit this firmware, using the board defaults");
            return false;
        }
        if (validate(g_board, g_stored.map, g_err, sizeof(g_err))) {
            LOG_ERR("saved pin map is not valid (%s), using the board defaults", g_err);
            return false;
        }
        out = g_stored.map;
        return true;
    }

    bool save(const Map &map)
    {
        g_stored = {MAGIC, (uint32_t)g_board.connectorCount, map};
        return PersistBlob::save(NVS_NS, NVS_KEY, &g_stored, sizeof(g_stored));
    }

    void routePcnt()
    {
        bool used[8] = {};
        for (size_t i = 0; i < FN_COUNT; i++) {
            Fn fn = (Fn)i;
            int unit = pcntUnit(g_active, fn);
            gpio_dt_spec a, b;
            if (unit < 0 || unit >= (int)ARRAY_SIZE(used) || !Pins::spec(fn, 0, a) || !Pins::spec(fn, 1, b)) {
                continue;
            }
            used[unit] = true;
            gpio_pin_configure_dt(&a, GPIO_INPUT);
            gpio_pin_configure_dt(&b, GPIO_INPUT);
            const Assignment &as = g_active.fn[i];
            esp_rom_gpio_connect_in_signal(gpioOf(g_board, g_active, fn, 0),
                                           PCNT_SIG_CH0_IN0_IDX + unit * PCNT_SIGNAL_STRIDE, as.options[0].invert);
            esp_rom_gpio_connect_in_signal(gpioOf(g_board, g_active, fn, 1),
                                           PCNT_CTRL_CH0_IN0_IDX + unit * PCNT_SIGNAL_STRIDE, as.options[1].invert);
        }
        for (int unit = 0; unit < g_board.pcntUnits && unit < (int)ARRAY_SIZE(used); unit++) {
            if (!used[unit]) {
                esp_rom_gpio_connect_in_signal(GPIO_MATRIX_CONST_ZERO_INPUT,
                                               PCNT_SIG_CH0_IN0_IDX + unit * PCNT_SIGNAL_STRIDE, false);
                esp_rom_gpio_connect_in_signal(GPIO_MATRIX_CONST_ZERO_INPUT,
                                               PCNT_CTRL_CH0_IN0_IDX + unit * PCNT_SIGNAL_STRIDE, false);
            }
        }
    }
}

namespace Pins
{
    void begin()
    {
        buildBoard();
        g_defaults = !loadSaved(g_active);
        if (g_defaults) {
            defaults(g_active);
        }
        if (validate(g_board, g_active, g_err, sizeof(g_err))) {
            LOG_ERR("pin map: %s", g_err);
        }
        routePcnt();
    }

    bool spec(Fn fn, size_t signal, gpio_dt_spec &out)
    {
        int gpio = gpioOf(g_board, g_active, fn, signal);
        if (gpio < 0 || gpio > 48 || isReserved(g_board, gpio)) {
            return false;
        }
        const SignalOptions &o = g_active.fn[(size_t)fn].options[signal];
        gpio_dt_flags_t flags = o.invert ? GPIO_ACTIVE_LOW : GPIO_ACTIVE_HIGH;
        if (def(fn).dir == Dir::In) {
            flags |= o.pull == Pull::Up ? GPIO_PULL_UP : (o.pull == Pull::Down ? GPIO_PULL_DOWN : 0);
        }
        out = {gpio < 32 ? GPIO0 : GPIO1, (gpio_pin_t)(gpio % 32), flags};
        return true;
    }

    Encoder encoder(Fn fn)
    {
        const Assignment &a = g_active.fn[(size_t)fn];
        return a.used ? a.encoder : Encoder::None;
    }

    int pcntUnit(Fn fn)
    {
        return PinMap::pcntUnit(g_active, fn);
    }

    bool usingDefaults()
    {
        return g_defaults;
    }

    int toJson(char *out, size_t outMax)
    {
        static Map saved;
        if (!loadSaved(saved)) {
            defaults(saved);
        }
        return PinMapJson::encode(g_board, saved, out, outMax);
    }

    const char *fromJson(char *json, size_t len)
    {
        if (Machine::state() != Machine::State::Idle || Mode::running()) {
            return "machine not idle";
        }
        static Map map;
        const char *refusal = PinMapJson::decode(g_board, json, len, map, g_err, sizeof(g_err));
        if (refusal) {
            return refusal;
        }
        return save(map) ? nullptr : "save failed";
    }

    const char *reset()
    {
        static Map map;
        defaults(map);
        return save(map) ? nullptr : "save failed";
    }
}
