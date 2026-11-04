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

// JSON messages of the /ws protocol (docs/protocol.md). Each build returns
// the length written, or -1 if it didn't fit.
namespace WsMessages
{
    int status(char *out, size_t outMax);
    int settings(char *out, size_t outMax);
    int wifi(char *out, size_t outMax);
    int scan(char *out, size_t outMax);
    int typed(char *out, size_t outMax, const char *type, const char *msg);

    // {"type":"cmd","line":...}: line points into json. False if malformed.
    bool parseCommand(char *json, size_t len, const char *&line);
}

#endif
