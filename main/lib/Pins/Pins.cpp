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
    // GPIO matrix input signals of PCNT unit n are 4 apart.
    constexpr uint32_t PCNT_SIGNAL_STRIDE = PCNT_SIG_CH0_IN1_IDX - PCNT_SIG_CH0_IN0_IDX;

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
        for (size_t i = 0; i < g_board.connectorCount; i++) {
            if (strlen(g_board.connectors[i].id) > ID_MAX) {
                LOG_ERR("connector id %s is too long to save", g_board.connectors[i].id);
            }
        }
        g_board.pcntUnits = DT_CHILD_NUM_STATUS_OKAY(DT_NODELABEL(pcnt));
    }

    void defaults(Map &out)
    {
        memset(&out, 0, sizeof(out));
        for (const DefaultPins &d : DEFAULTS) {
            Assignment a;
            if (!d.count) {
                continue;
            }
            if (fromGpios(g_board, d.fn, d.gpios, d.count, a)) {
                add(out, a);
            } else {
                LOG_ERR("default pins of %s are not next to each other in one connector", def(d.fn).id);
            }
        }
    }

    void warnIgnored(const char *msg)
    {
        LOG_WRN("pin map: %s, ignored", msg);
    }

    // The saved assignments that are known to this firmware and board, not validated.
    bool loadSaved(Map &out)
    {
        memset(&out, 0, sizeof(out));
        size_t n = PersistBlob::load(NVS_NS, NVS_KEY, &g_stored, sizeof(g_stored));
        if (n == 0) {
            return false;
        }
        if (n != sizeof(g_stored) || !fromStored(g_board, g_stored, out, warnIgnored)) {
            LOG_ERR("saved pin map does not fit this firmware, using the board defaults");
            return false;
        }
        return true;
    }

    bool save(const Map &map)
    {
        toStored(g_board, map, g_stored);
        return PersistBlob::save(NVS_NS, NVS_KEY, &g_stored, sizeof(g_stored));
    }

    gpio_dt_spec specOf(const Assignment &a, size_t signal)
    {
        int gpio = gpioOf(g_board, a, signal);
        if (gpio < 0 || gpio > 48 || isReserved(g_board, gpio)) {
            return {};
        }
        const SignalOptions &o = a.options[signal];
        gpio_dt_flags_t flags = o.invert ? GPIO_ACTIVE_LOW : GPIO_ACTIVE_HIGH;
        if (def(a.fn).dir == Dir::In) {
            flags |= o.pull == Pull::Up ? GPIO_PULL_UP : (o.pull == Pull::Down ? GPIO_PULL_DOWN : 0);
        }
        return {gpio < 32 ? GPIO0 : GPIO1, (gpio_pin_t)(gpio % 32), flags};
    }

    void routePcnt()
    {
        bool used[8] = {};
        for (size_t i = 0; i < g_active.count; i++) {
            const Assignment &as = g_active.list[i];
            int unit = pcntUnit(g_active, as.fn);
            gpio_dt_spec a = specOf(as, 0);
            gpio_dt_spec b = specOf(as, 1);
            if (unit < 0 || unit >= (int)ARRAY_SIZE(used) || !a.port || !b.port) {
                continue;
            }
            used[unit] = true;
            gpio_pin_configure_dt(&a, GPIO_INPUT);
            gpio_pin_configure_dt(&b, GPIO_INPUT);
            uint32_t gpioA = gpioOf(g_board, as, 0);
            uint32_t gpioB = gpioOf(g_board, as, 1);
            bool invA = as.options[0].invert;
            bool invB = as.options[1].invert;
            uint32_t offset = unit * PCNT_SIGNAL_STRIDE;
            esp_rom_gpio_connect_in_signal(gpioA, PCNT_SIG_CH0_IN0_IDX + offset, invA);
            esp_rom_gpio_connect_in_signal(gpioB, PCNT_CTRL_CH0_IN0_IDX + offset, invB);
            esp_rom_gpio_connect_in_signal(gpioB, PCNT_SIG_CH1_IN0_IDX + offset, invB);
            esp_rom_gpio_connect_in_signal(gpioA, PCNT_CTRL_CH1_IN0_IDX + offset, invA);
        }
        for (int unit = 0; unit < g_board.pcntUnits && unit < (int)ARRAY_SIZE(used); unit++) {
            if (!used[unit]) {
                const uint32_t sigs[] = {PCNT_SIG_CH0_IN0_IDX, PCNT_CTRL_CH0_IN0_IDX, PCNT_SIG_CH1_IN0_IDX,
                                         PCNT_CTRL_CH1_IN0_IDX};
                uint32_t offset = unit * PCNT_SIGNAL_STRIDE;
                for (uint32_t sig : sigs) {
                    esp_rom_gpio_connect_in_signal(GPIO_MATRIX_CONST_ZERO_INPUT, sig + offset, false);
                }
            }
        }
    }
}

namespace Pins
{
    void begin()
    {
        static Map saved;
        buildBoard();
        g_defaults = !loadSaved(saved);
        if (g_defaults) {
            defaults(saved);
        }
        keepValid(g_board, saved, g_active, warnIgnored);
        if (missingRequired(g_active, g_err, sizeof(g_err))) {
            LOG_ERR("pin map: %s", g_err);
        }
        routePcnt();
    }

    size_t count(Fn fn)
    {
        return countOf(g_active, fn);
    }

    bool spec(Fn fn, size_t nth, size_t signal, gpio_dt_spec &out)
    {
        const Assignment *a = find(g_active, fn, nth);
        out = a ? specOf(*a, signal) : gpio_dt_spec{};
        return out.port != nullptr;
    }

    bool spec(Fn fn, size_t signal, gpio_dt_spec &out)
    {
        return spec(fn, 0, signal, out);
    }

    Encoder encoder(Fn fn)
    {
        const Assignment *a = find(g_active, fn);
        return a ? a->encoder : Encoder::None;
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
