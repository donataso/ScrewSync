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
#include "Console.hpp"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <zephyr/device.h>
#include <zephyr/drivers/uart.h>
#include <zephyr/kernel.h>
#include "Command.hpp"
#include "DisplayImage.hpp"

#ifdef CONFIG_ESP_SPIRAM
#define EXT_RAM_BSS __attribute__((section(".ext_ram.bss")))
#else
#define EXT_RAM_BSS
#endif

namespace
{
    constexpr const char *UPLOAD_PREFIX = "display upload ";
    constexpr uint32_t UPLOAD_IDLE_TIMEOUT_MS = 5000;

    // Blocks the main loop while the image streams in; the host waits on USB until it's read.
    const char *receiveImage(const struct device *dev, uint32_t len)
    {
        uint8_t buf[256];
        uint32_t received = 0;
        int64_t lastByteMs = k_uptime_get();
        while (received < len)
        {
            size_t n = 0;
            unsigned char c;
            while (n < sizeof(buf) && received + n < len && uart_poll_in(dev, &c) == 0) {
                buf[n++] = c;
            }
            if (n)
            {
                if (!DisplayImage::storeChunk(buf, n)) {
                    return "flash write failed";
                }
                received += n;
                lastByteMs = k_uptime_get();
            }
            else if (k_uptime_get() - lastByteMs > UPLOAD_IDLE_TIMEOUT_MS) {
                return "timed out";
            }
            else {
                k_usleep(100);
            }
        }
        return nullptr;
    }

    // "display upload <len> <crc32 hex>": raw image bytes follow "ok upload ready".
    void handleUpload(const struct device *dev, const char *args)
    {
        char *end;
        uint32_t len = strtoul(args, &end, 10);
        uint32_t crc = strtoul(end, &end, 16);
        DisplayImage::Info stored = DisplayImage::info();
        if (stored.stored && stored.length == len && stored.crc == crc) {
            printf("ok upload skipped, already stored\n");
        }
        else
        {
            const char *reason = DisplayImage::storeBegin(len, crc);
            if (reason)
            {
                printf("err display upload: %s\n", reason);
                return;
            }
            printf("ok upload ready\n");
            reason = receiveImage(dev, len);
            if (reason) {
                DisplayImage::storeAbort();
            } else {
                reason = DisplayImage::storeFinish();
            }
            if (reason)
            {
                printf("err display upload: %s\n", reason);
                return;
            }
        }
        const char *refusal = DisplayImage::flashStored();
        if (refusal) {
            printf("err display flash: %s\n", refusal);
        } else {
            printf("ok display flash started\n");
        }
    }
}

void consolePoll()
{
    static const struct device *dev = DEVICE_DT_GET(DT_CHOSEN(zephyr_console));
    static char buf[160];
    static uint8_t len = 0;
    static char out[Command::MAX_REPLY_LEN] EXT_RAM_BSS;

    unsigned char c;
    while (uart_poll_in(dev, &c) == 0)
    {
        if (c == '\r') {
            continue;
        }
        if (c != '\n' && len < sizeof(buf) - 1)
        {
            buf[len++] = (char)c;
            continue;
        }
        buf[len] = '\0';
        len = 0;
        if (!buf[0]) {
            continue;
        }

        if (!strncmp(buf, UPLOAD_PREFIX, strlen(UPLOAD_PREFIX)))
        {
            handleUpload(dev, buf + strlen(UPLOAD_PREFIX));
            continue;
        }
        Command::apply(buf, out, sizeof(out));
        printf("%s\n", out);
    }
}
