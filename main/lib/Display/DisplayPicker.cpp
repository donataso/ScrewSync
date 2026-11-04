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
#include "DisplayInternal.hpp"
#include "ThreadStore.hpp"
#include <ctype.h>
#include <stdio.h>
#include <string.h>

using namespace Display::detail;
using namespace Display::theme;
using namespace Nextion;

namespace
{
    constexpr size_t GROUPS_MAX = ThreadTable::MAX_GROUPS * ThreadTable::GROUP_NAME_MAX;

    char g_group[ThreadTable::GROUP_NAME_MAX];

    // Tab label: short standards upper case ("UNC"), others capitalized.
    void groupLabel(const char *name, char *out, size_t outMax)
    {
        size_t len = strlen(name);
        size_t i = 0;
        for (; i < len && i + 1 < outMax; i++) {
            out[i] = len <= 4 || i == 0 ? (char)toupper((unsigned char)name[i]) : name[i];
        }
        out[i] = '\0';
    }

    // "M10x1.25" -> "M10×1.25".
    void designationLabel(const char *designation, char *out, size_t outMax)
    {
        size_t n = 0;
        for (const char *p = designation; *p && n + 3 < outMax; p++)
        {
            bool times = *p == 'x' && p > designation && isdigit((unsigned char)p[-1]) && isdigit((unsigned char)p[1]);
            if (times) {
                out[n++] = '\xC3';
                out[n++] = '\x97';
            } else {
                out[n++] = *p;
            }
        }
        out[n] = '\0';
    }

    // Next comma-separated item of list into out; advances list.
    bool nextItem(const char *&list, char *out, size_t outMax)
    {
        if (!*list) {
            out[0] = '\0';
            return false;
        }
        size_t len = strcspn(list, ",");
        snprintf(out, outMax, "%.*s", (int)len, list);
        list += len + (list[len] == ',');
        return true;
    }

    bool hasItem(const char *list, const char *name)
    {
        char item[ThreadTable::DESIGNATION_MAX];
        while (nextItem(list, item, sizeof(item))) {
            if (!strcmp(item, name)) {
                return true;
            }
        }
        return false;
    }

    void firstGroup(const char *groups, char (&out)[ThreadTable::GROUP_NAME_MAX])
    {
        nextItem(groups, out, sizeof(out));
    }

    // Returns whether any tab's command changed.
    bool pushTabs()
    {
        static char groups[GROUPS_MAX];
        ThreadStore::groups(groups, sizeof(groups));
        if (!hasItem(groups, g_group)) {
            firstGroup(groups, g_group);
        }
        bool changed = false;
        char key[KEY_MAX];
        char item[ThreadTable::DESIGNATION_MAX];
        char label[ThreadTable::DESIGNATION_MAX + 4];
        const char *list = groups;
        for (size_t i = 0; i < PICKER_TABS; i++)
        {
            nextItem(list, item, sizeof(item));
            groupLabel(item, label, sizeof(label));
            snprintf(key, sizeof(key), "tab%u.txt", (unsigned)i);
            pushText(key, "%s", label);
            snprintf(key, sizeof(key), "tabCmd%u.txt", (unsigned)i);
            changed |= pushText(key, "%s", item);
            snprintf(key, sizeof(key), "tab%u", (unsigned)i);
            pushSelected(key, *item && !strcmp(item, g_group));
        }
        pushText("pickerGroup.txt", "%s", g_group);
        return changed;
    }

    // Returns whether any entry's command changed.
    bool pushEntries()
    {
        char params[256];
        char group[ThreadTable::GROUP_NAME_MAX];
        char preset[ThreadTable::DESIGNATION_MAX];
        Mode::describe(Mode::Id::Thread, params, sizeof(params));
        bool inGroup = paramValue(params, "group", group, sizeof(group)) && !strcmp(group, g_group);
        if (!paramValue(params, "preset", preset, sizeof(preset))) {
            preset[0] = '\0';
        }

        static char entries[PICKER_CELLS * ThreadTable::DESIGNATION_MAX];
        size_t total;
        size_t next;
        if (!ThreadStore::list(g_group, 0, entries, sizeof(entries), total, next)) {
            entries[0] = '\0';
        }
        bool changed = false;
        char key[KEY_MAX];
        char item[ThreadTable::DESIGNATION_MAX];
        char label[ThreadTable::DESIGNATION_MAX + 4];
        const char *list = entries;
        for (size_t i = 0; i < PICKER_CELLS; i++)
        {
            nextItem(list, item, sizeof(item));
            designationLabel(item, label, sizeof(label));
            snprintf(key, sizeof(key), "g%u.txt", (unsigned)i);
            pushText(key, "%s", label);
            snprintf(key, sizeof(key), "gCmd%u.txt", (unsigned)i);
            changed |= pushText(key, "%s", item);
            snprintf(key, sizeof(key), "g%u", (unsigned)i);
            pushSelected(key, inGroup && *item && !strcmp(item, preset));
        }
        return changed;
    }
}

namespace Display::detail
{
    // Opens on the selected thread's group, else the first.
    void openPicker()
    {
        char params[256];
        Mode::describe(Mode::Id::Thread, params, sizeof(params));
        if (!paramValue(params, "group", g_group, sizeof(g_group)) || !strcmp(g_group, "-")) {
            char groups[GROUPS_MAX];
            ThreadStore::groups(groups, sizeof(groups));
            firstGroup(groups, g_group);
        }
    }

    void pushPicker()
    {
        bool changed = pushTabs();
        changed |= pushEntries();
        if (changed) {
            writeCommand("click pickerSync,1");
        }
    }

    void selectPickerGroup(const char *group)
    {
        snprintf(g_group, sizeof(g_group), "%s", group);
        pushSoon();
    }
}
