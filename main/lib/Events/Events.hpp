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
#ifndef Events_h
#define Events_h

#include <stddef.h>
#include <stdint.h>
#include <zephyr/zbus/zbus.h>

// zbus channels. Listeners run in the publisher's thread: keep them short, no blocking.
namespace Events
{
    struct Running
    {
        bool running;
    };

    // Stepper::Axis
    struct AxisArrived
    {
        uint8_t axis;
    };

    struct SettingsChanged
    {
        uint8_t unused;
    };

    constexpr size_t REFUSAL_MAX = 64;

    // A refused command from the display or a switch, without "err ".
    struct Refusal
    {
        char text[REFUSAL_MAX];
    };

    void publish(const struct zbus_channel *chan, const void *msg);
    void publishRefusal(const char *reply);
}

ZBUS_CHAN_DECLARE(RUNNING_CHAN, AXIS_ARRIVED_CHAN, SETTINGS_CHAN, REFUSAL_CHAN);

#endif
