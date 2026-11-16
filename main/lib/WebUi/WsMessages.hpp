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
#ifndef WsMessages_h
#define WsMessages_h

#include <stddef.h>
#include <stdint.h>

// JSON messages of the /ws protocol (docs/protocol.md). Each build returns
// the length written, or -1 if it didn't fit.
namespace WsMessages
{
    // No "id" in the message: a broadcast, or a request without an id.
    constexpr int32_t NO_ID = -1;

    int status(char *out, size_t outMax);
    int settings(char *out, size_t outMax, int32_t id = NO_ID);
    int wifi(char *out, size_t outMax, int32_t id = NO_ID);
    int scan(char *out, size_t outMax);
    int capabilities(char *out, size_t outMax, bool withType);
    int inputs(char *out, size_t outMax);
    int typed(char *out, size_t outMax, const char *type, const char *msg, int32_t id = NO_ID);

    // {"type":"cmd","line":...[,"id":n]}: line points into json. False if malformed.
    bool parseCommand(char *json, size_t len, const char *&line, int32_t &id);
}

#endif
