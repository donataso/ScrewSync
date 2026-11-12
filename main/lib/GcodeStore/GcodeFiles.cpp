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
#include "GcodeFiles.hpp"
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <zephyr/fs/fs.h>
#include <zephyr/fs/littlefs.h>
#include <zephyr/logging/log.h>
#include <zephyr/storage/flash_map.h>

LOG_MODULE_REGISTER(gcode_files, LOG_LEVEL_INF);

namespace
{
    constexpr const char *MOUNT_POINT = "/gcode";
    constexpr size_t HEADER_BUF_MAX = 256;
    constexpr size_t PATH_MAX_LEN = 48;

    FS_LITTLEFS_DECLARE_DEFAULT_CONFIG(g_lfsData);
    struct fs_mount_t g_mnt = {
        .type = FS_LITTLEFS,
        .mnt_point = MOUNT_POINT,
        .fs_data = &g_lfsData,
        .storage_dev = (void *)PARTITION_ID(gcode_partition),
    };
    bool g_mounted = false;
    char g_headerBuf[HEADER_BUF_MAX]; // too big for the stack

    void path(const char *id, const char *ext, char *out, size_t outMax)
    {
        snprintf(out, outMax, "%s/%s.%s", MOUNT_POINT, id, ext);
    }

    bool isHex(const char *s, size_t len)
    {
        for (size_t i = 0; i < len; i++) {
            char c = s[i];
            if (!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f'))) {
                return false;
            }
        }
        return true;
    }

    // "<8 hex digits>.gc" only; skips ".tmp" files left by an interrupted save.
    bool isProgramFile(const char *name, uint32_t &id)
    {
        if (strlen(name) != GcodeStore::ID_LEN + 3 || strcmp(name + GcodeStore::ID_LEN, ".gc") != 0 ||
            !isHex(name, GcodeStore::ID_LEN)) {
            return false;
        }
        id = (uint32_t)strtoul(name, nullptr, 16);
        return true;
    }

    bool readHeader(const char *filePath, GcodeMeta::Header &h)
    {
        struct fs_file_t f;
        fs_file_t_init(&f);
        if (fs_open(&f, filePath, FS_O_READ) != 0) {
            return false;
        }
        ssize_t n = fs_read(&f, g_headerBuf, sizeof(g_headerBuf) - 1);
        fs_close(&f);
        if (n <= 0) {
            return false;
        }
        g_headerBuf[n] = '\0';
        return GcodeMeta::decodeHeader(g_headerBuf, (size_t)n, h);
    }

    template <size_t N>
    void copyField(char (&out)[N], const char *s, size_t len)
    {
        len = len < N - 1 ? len : N - 1;
        memcpy(out, s, len);
        out[len] = '\0';
    }

    void fillInfo(GcodeStore::ProgramInfo &info, const GcodeMeta::Header &h, size_t fileSize)
    {
        copyField(info.name, h.name, h.nameLen);
        copyField(info.flavor, h.flavor, h.flavorLen);
        info.size = fileSize - h.bodyOffset;
    }
}

namespace GcodeFiles
{
    bool mount()
    {
        if (g_mounted) {
            return true;
        }
        int rc = fs_mount(&g_mnt);
        if (rc != 0) {
            LOG_ERR("mount failed: %d", rc);
            return false;
        }
        g_mounted = true;
        return true;
    }

    bool validId(const char *id)
    {
        return strlen(id) == GcodeStore::ID_LEN && isHex(id, GcodeStore::ID_LEN);
    }

    void nextId(char (&out)[GcodeStore::ID_LEN + 1])
    {
        uint32_t maxId = 0;
        struct fs_dir_t dir;
        fs_dir_t_init(&dir);
        if (fs_opendir(&dir, MOUNT_POINT) == 0) {
            struct fs_dirent entry;
            while (fs_readdir(&dir, &entry) == 0 && entry.name[0]) {
                uint32_t id;
                if (entry.type == FS_DIR_ENTRY_FILE && isProgramFile(entry.name, id) && id > maxId) {
                    maxId = id;
                }
            }
            fs_closedir(&dir);
        }
        snprintf(out, sizeof(out), "%08x", maxId + 1);
    }

    bool save(const char *id, const char *flavor, const char *name, const char *text, size_t len)
    {
        char tmpPath[PATH_MAX_LEN];
        char finalPath[PATH_MAX_LEN];
        path(id, "tmp", tmpPath, sizeof(tmpPath));
        path(id, "gc", finalPath, sizeof(finalPath));

        char header[GcodeMeta::NAME_MAX_LEN + GcodeStore::FLAVOR_MAX_LEN + 3];
        int headerLen = GcodeMeta::encodeHeader(flavor, name, header, sizeof(header));
        if (headerLen < 0) {
            return false;
        }

        struct fs_file_t f;
        fs_file_t_init(&f);
        if (fs_open(&f, tmpPath, FS_O_CREATE | FS_O_WRITE | FS_O_TRUNC) != 0) {
            return false;
        }
        bool ok = fs_write(&f, header, (size_t)headerLen) == (ssize_t)headerLen;
        if (ok && len) {
            ok = fs_write(&f, text, len) == (ssize_t)len;
        }
        fs_close(&f);
        if (!ok || fs_rename(tmpPath, finalPath) != 0) {
            fs_unlink(tmpPath);
            return false;
        }
        return true;
    }

    const char *load(const char *id, GcodeStore::ProgramInfo &info, uint8_t *body, size_t bodyMax)
    {
        if (!mount()) {
            return "no filesystem";
        }
        if (!validId(id)) {
            return "bad id";
        }

        char filePath[PATH_MAX_LEN];
        path(id, "gc", filePath, sizeof(filePath));
        struct fs_dirent st;
        if (fs_stat(filePath, &st) != 0) {
            return "not found";
        }
        GcodeMeta::Header h;
        if (!readHeader(filePath, h) || h.bodyOffset > st.size) {
            return "corrupt program file";
        }
        if (st.size - h.bodyOffset > bodyMax) {
            return "program too large";
        }
        memcpy(info.id, id, sizeof(info.id));
        fillInfo(info, h, st.size);

        struct fs_file_t f;
        fs_file_t_init(&f);
        if (fs_open(&f, filePath, FS_O_READ) != 0) {
            return "open failed";
        }
        fs_seek(&f, (off_t)h.bodyOffset, FS_SEEK_SET);
        ssize_t n = info.size ? fs_read(&f, body, info.size) : 0;
        fs_close(&f);
        if (n < 0 || (size_t)n != info.size) {
            return "read failed";
        }
        return nullptr;
    }

    bool list(GcodeStore::ListResult &out)
    {
        out.count = 0;
        out.freeBytes = 0;
        if (!mount()) {
            return false;
        }
        struct fs_dir_t dir;
        fs_dir_t_init(&dir);
        if (fs_opendir(&dir, MOUNT_POINT) != 0) {
            return false;
        }

        struct fs_dirent entry;
        while (out.count < GcodeStore::LIST_MAX && fs_readdir(&dir, &entry) == 0 && entry.name[0]) {
            uint32_t id;
            if (entry.type != FS_DIR_ENTRY_FILE || !isProgramFile(entry.name, id)) {
                continue;
            }
            GcodeStore::ProgramInfo &p = out.programs[out.count];
            snprintf(p.id, sizeof(p.id), "%08x", id);
            char filePath[PATH_MAX_LEN];
            path(p.id, "gc", filePath, sizeof(filePath));
            GcodeMeta::Header h;
            if (!readHeader(filePath, h) || h.bodyOffset > entry.size) {
                continue;
            }
            fillInfo(p, h, entry.size);
            out.count++;
        }
        fs_closedir(&dir);

        struct fs_statvfs vfs;
        if (fs_statvfs(MOUNT_POINT, &vfs) == 0) {
            out.freeBytes = (uint64_t)vfs.f_bfree * vfs.f_frsize;
        }
        return true;
    }

    const char *remove(const char *id)
    {
        char filePath[PATH_MAX_LEN];
        path(id, "gc", filePath, sizeof(filePath));
        int rc = fs_unlink(filePath);
        if (rc != 0) {
            return rc == -ENOENT ? "not found" : "delete failed";
        }
        return nullptr;
    }
}
