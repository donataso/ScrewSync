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
#include "Command.hpp"
#include "CommandInternal.hpp"
#include "Mode.hpp"
#include "ProbeInput.hpp"
#include "ThreadStore.hpp"

namespace
{
    using Command::Detail::skipSpaces;

    void replyDescribe(Mode::Id id, char *out, size_t outMax)
    {
        int n = snprintf(out, outMax, "ok ");
        Mode::describe(id, out + n, outMax - (size_t)n);
    }

    void modeParams(Mode::Id id, const char *args, char *out, size_t outMax)
    {
        const char *a = skipSpaces(args);
        if (!*a || !strcmp(a, "status")) {
            replyDescribe(id, out, outMax);
            return;
        }
        char param[16];
        size_t len = strcspn(a, " ");
        const char *value = skipSpaces(a + len);
        if (len >= sizeof(param) || !*value) {
            snprintf(out, outMax, "err %s <param> <value>", Mode::name(id));
            return;
        }
        memcpy(param, a, len);
        param[len] = '\0';
        const char *refusal = Mode::setParam(id, param, value);
        if (refusal) {
            snprintf(out, outMax, "err %s %s: %s", Mode::name(id), param, refusal);
        } else {
            snprintf(out, outMax, "ok %s=%s", param, value);
        }
    }

    void threadList(const char *args, char *out, size_t outMax)
    {
        char group[ThreadTable::GROUP_NAME_MAX];
        unsigned long offset = 0;
        if (sscanf(args, "%11s %lu", group, &offset) < 1) {
            snprintf(out, outMax, "err threads list <group> [<offset>]");
            return;
        }
        char items[Command::MAX_REPLY_LEN - 64];
        size_t total;
        size_t next;
        if (!ThreadStore::list(group, offset, items, sizeof(items), total, next)) {
            snprintf(out, outMax, "err unknown group '%s'", group);
        } else {
            snprintf(out, outMax, "ok total=%u next=%u items=%s", (unsigned)total, (unsigned)next, items);
        }
    }
}

namespace Command::Detail
{
    void mode(const char *args, char *out, size_t outMax)
    {
        const char *a = skipSpaces(args);
        if (!*a || !strcmp(a, "status")) {
            Mode::Progress p = Mode::progress();
            const char *error = Mode::lastError();
            int n = snprintf(out, outMax, "ok mode=%s running=%d phase=%s pass=%ld/%ld error=%s modes=",
                             Mode::name(Mode::current()), (int)Mode::running(), *p.phase ? p.phase : "-",
                             (long)p.pass, (long)p.passes, *error ? error : "-");
            for (size_t i = 0; i < Mode::COUNT && n >= 0 && (size_t)n < outMax; i++) {
                n += snprintf(out + n, outMax - (size_t)n, i ? ",%s" : "%s", Mode::name((Mode::Id)i));
            }
            return;
        }
        Mode::Id id;
        if (!Mode::parse(a, id)) {
            snprintf(out, outMax, "err unknown mode '%s'", a);
            return;
        }
        reply(out, outMax, Mode::select(id), "ok mode=%s", a);
    }

    void start(const char *args, char *out, size_t outMax)
    {
        const char *a = skipSpaces(args);
        if (*a && strcmp(a, "zero") != 0) {
            snprintf(out, outMax, "err start [zero]");
            return;
        }
        reply(out, outMax, Mode::start(*a != '\0'), "ok %s started", Mode::name(Mode::current()));
    }

    void stop(const char *, char *out, size_t outMax)
    {
        Mode::stop();
        snprintf(out, outMax, "ok stopped");
    }

    void skip(const char *, char *out, size_t outMax)
    {
        reply(out, outMax, Mode::skip(), "ok %s", "skipping a pass");
    }

    void override(const char *args, char *out, size_t outMax)
    {
        const char *a = skipSpaces(args);
        int32_t pct = Mode::feedOverride();
        const char *refusal = nullptr;
        if (!strcmp(a, "+")) {
            refusal = Mode::setFeedOverride(pct + Mode::OVERRIDE_STEP_PCT);
        } else if (!strcmp(a, "-")) {
            refusal = Mode::setFeedOverride(pct - Mode::OVERRIDE_STEP_PCT);
        } else if (!strcmp(a, "reset")) {
            refusal = Mode::setFeedOverride(100);
        } else if (*a && strcmp(a, "status") != 0) {
            snprintf(out, outMax, "err override [status] | + | - | reset");
            return;
        }
        if (refusal) {
            snprintf(out, outMax, "err %s", refusal);
        } else {
            snprintf(out, outMax, "ok override=%ld", (long)Mode::feedOverride());
        }
    }

    void probe(const char *args, char *out, size_t outMax)
    {
        const char *a = skipSpaces(args);
        if (!strncmp(a, "go ", 3)) {
            const char *dir = skipSpaces(a + 3);
            Stepper::Axis axis;
            bool reverse;
            if (!strcmp(dir, "z-")) {
                axis = Stepper::Axis::Z;
                reverse = false;
            } else if (!strcmp(dir, "z+")) {
                axis = Stepper::Axis::Z;
                reverse = true;
            } else if (!strcmp(dir, "x-")) {
                axis = Stepper::Axis::X;
                reverse = false;
            } else if (!strcmp(dir, "x+")) {
                axis = Stepper::Axis::X;
                reverse = true;
            } else {
                snprintf(out, outMax, "err probe go z-|z+|x-|x+");
                return;
            }
            reply(out, outMax, Mode::probeGo(axis, reverse), "ok %s", "probe started");
            return;
        }
        if (!*a || !strcmp(a, "status")) {
            int n = snprintf(out, outMax, "ok ");
            Mode::describe(Mode::Id::Probe, out + n, outMax - (size_t)n);
            n = (int)strlen(out);
            snprintf(out + n, outMax - (size_t)n, " contact=%d", (int)ProbeInput::contact());
            return;
        }
        modeParams(Mode::Id::Probe, args, out, outMax);
    }

    void gear(const char *args, char *out, size_t outMax)
    {
        modeParams(Mode::Id::Gear, args, out, outMax);
    }
    void async(const char *args, char *out, size_t outMax)
    {
        modeParams(Mode::Id::Async, args, out, outMax);
    }
    void turn(const char *args, char *out, size_t outMax)
    {
        modeParams(Mode::Id::Turn, args, out, outMax);
    }
    void part(const char *args, char *out, size_t outMax)
    {
        modeParams(Mode::Id::Part, args, out, outMax);
    }
    void slot(const char *args, char *out, size_t outMax)
    {
        modeParams(Mode::Id::Slot, args, out, outMax);
    }
    void ellipse(const char *args, char *out, size_t outMax)
    {
        modeParams(Mode::Id::Ellipse, args, out, outMax);
    }

    void thread(const char *args, char *out, size_t outMax)
    {
        const char *a = skipSpaces(args);
        if (strncmp(a, "preset ", 7) != 0) {
            modeParams(Mode::Id::Thread, args, out, outMax);
            return;
        }
        char group[ThreadTable::GROUP_NAME_MAX];
        char designation[ThreadTable::DESIGNATION_MAX];
        if (sscanf(a + 7, "%11s %15s", group, designation) != 2) {
            snprintf(out, outMax, "err thread preset <group> <designation>");
            return;
        }
        const char *refusal = Mode::threadPreset(group, designation);
        if (refusal) {
            snprintf(out, outMax, "err thread preset: %s", refusal);
        } else {
            replyDescribe(Mode::Id::Thread, out, outMax);
        }
    }

    void threads(const char *args, char *out, size_t outMax)
    {
        const char *a = skipSpaces(args);
        if (!*a || !strcmp(a, "groups")) {
            int n = snprintf(out, outMax, "ok groups=");
            ThreadStore::groups(out + n, outMax - (size_t)n);
        } else if (!strncmp(a, "list ", 5)) {
            threadList(a + 5, out, outMax);
        } else if (!strcmp(a, "reset")) {
            if (refuseUnlessIdle(out, outMax)) {
                return;
            }
            ThreadStore::reset();
            snprintf(out, outMax, "ok threads reset");
        } else {
            snprintf(out, outMax, "err threads groups | list <group> [<offset>] | reset");
        }
    }
}
