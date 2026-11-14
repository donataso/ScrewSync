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
#include "Events.hpp"
#include <string.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(events, LOG_LEVEL_INF);

using namespace Events;

ZBUS_CHAN_DEFINE(RUNNING_CHAN, Running, nullptr, nullptr, ZBUS_OBSERVERS_EMPTY, ZBUS_MSG_INIT(false));
ZBUS_CHAN_DEFINE(AXIS_ARRIVED_CHAN, AxisArrived, nullptr, nullptr, ZBUS_OBSERVERS_EMPTY, ZBUS_MSG_INIT(0));
ZBUS_CHAN_DEFINE(SETTINGS_CHAN, SettingsChanged, nullptr, nullptr, ZBUS_OBSERVERS_EMPTY, ZBUS_MSG_INIT(0));
ZBUS_CHAN_DEFINE(REFUSAL_CHAN, Refusal, nullptr, nullptr, ZBUS_OBSERVERS_EMPTY, ZBUS_MSG_INIT({0}));

namespace Events
{
    constexpr k_timeout_t PUBLISH_TIMEOUT = K_MSEC(100);

    void publish(const struct zbus_channel *chan, const void *msg)
    {
        int err = zbus_chan_pub(chan, msg, PUBLISH_TIMEOUT);
        if (err) {
            LOG_WRN("publish failed: %d", err);
        }
    }

    void publishRefusal(const char *reply)
    {
        if (!strncmp(reply, "err ", 4)) {
            reply += 4;
        }
        Refusal r = {};
        strncpy(r.text, reply, sizeof(r.text) - 1);
        publish(&REFUSAL_CHAN, &r);
    }
}
