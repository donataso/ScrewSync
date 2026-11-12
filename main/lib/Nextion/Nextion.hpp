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
#ifndef Nextion_h
#define Nextion_h

#include <stddef.h>
#include <stdint.h>
#include <zephyr/device.h>

// A Nextion display on a UART: framed commands and return data, page
// attributes written only when changed, and the whmi-wri / twfile
// transfers (docs/protocol.md "Display UI upload"). Heartbeats write the
// linkAge global of display/program.nx.
namespace Nextion
{
    struct Config
    {
        const struct device *uart;
        uint32_t linkBaud;
        uint32_t uploadBaud;
        // A received frame without its 0xFF 0xFF 0xFF terminator.
        void (*onFrame)(const uint8_t *data, size_t len);
        // Every link pass, on the link thread, while no transfer holds the UART.
        void (*onService)();
        // A transfer gave the UART back; restarted = the display may have restarted.
        void (*onResumed)(bool restarted);
    };

    // The UART and RX; writes are safe before start().
    void begin(const Config &config);
    void start();

    // From the link thread, or while holding pause().
    void writeRaw(const char *cmd);
    // writeRaw() plus a heartbeat every few hundred bytes, so a long push doesn't age the link.
    void writeCommand(const char *cmd);
    void writeHeartbeat();

    constexpr size_t KEY_MAX = 24;
    // Each returns whether it was written (the value changed).
    bool pushAttr(const char *key, const char *value);
    bool pushNum(const char *key, int value);
    __attribute__((format(printf, 2, 3))) bool pushText(const char *key, const char *fmt, ...);
    void pushColor(const char *component, const char *attr, int color);
    // Quotes and backslashes can't be escaped in a Nextion string.
    char safeChar(char c);
    // Attributes pushed from now on belong to page.
    void setPage(uint8_t page);
    uint8_t page();
    // Forget what was sent: everything is pushed again.
    void resync();
    void forgetPage(uint8_t page);

    // Takes the UART from the link thread; false while another transfer has it.
    bool pause();
    void release();
    // A file into the display's filesystem. Caller holds pause().
    bool twfile(const char *path, const uint8_t *data, size_t len);

    // Connects at whichever known baud the display answers, then starts a
    // whmi-wri upload of totalLen bytes. The link stays paused until finish/abort.
    bool flashBegin(uint32_t totalLen);
    bool flashChunk(const uint8_t *data, size_t len);
    bool flashFinish();
    void flashAbort();
}

#endif
