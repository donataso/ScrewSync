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
#include <zephyr/ztest.h>
#include <stdio.h>
#include <string.h>
#include "Nextion.hpp"
#include "NextionFrame.hpp"

using Nextion::FrameParser;
using Event = Nextion::FrameParser::Event;

namespace
{
    char g_lastCommand[128];
    int g_commands;

    Event feedAll(FrameParser &parser, const uint8_t *bytes, size_t len)
    {
        Event last = Event::None;
        for (size_t i = 0; i < len; i++) {
            last = parser.feed(bytes[i]);
            if (i + 1 < len) {
                zassert_equal(last, Event::None, "frame ended at byte %zu", i);
            }
        }
        return last;
    }

    void resetAttrs(void *)
    {
        Nextion::resync();
        Nextion::setPage(0);
        g_commands = 0;
        g_lastCommand[0] = '\0';
    }
}

// The link's write, replaced: the attribute cache needs no UART.
namespace Nextion
{
    void writeCommand(const char *cmd)
    {
        snprintf(g_lastCommand, sizeof(g_lastCommand), "%s", cmd);
        g_commands++;
    }
}

ZTEST_SUITE(nextion_frame, NULL, NULL, NULL, NULL, NULL);

ZTEST(nextion_frame, test_frame_ends_at_three_ff)
{
    FrameParser parser;
    const uint8_t bytes[] = {0x7C, 'o', 'k', 0xFF, 0xFF, 0xFF};
    zassert_equal(feedAll(parser, bytes, sizeof(bytes)), Event::Frame);
    zassert_equal(parser.len, 3);
    zassert_mem_equal(parser.data, bytes, 3);
}

ZTEST(nextion_frame, test_fewer_ff_are_data)
{
    FrameParser parser;
    const uint8_t bytes[] = {0x71, 0xFF, 0x01, 0xFF, 0xFF, 0x02, 0xFF, 0xFF, 0xFF};
    zassert_equal(feedAll(parser, bytes, sizeof(bytes)), Event::Frame);
    zassert_equal(parser.len, 6);
    zassert_mem_equal(parser.data, bytes, 6);
}

ZTEST(nextion_frame, test_empty_frame)
{
    FrameParser parser;
    const uint8_t bytes[] = {0xFF, 0xFF, 0xFF};
    zassert_equal(feedAll(parser, bytes, sizeof(bytes)), Event::Frame);
    zassert_equal(parser.len, 0);
}

ZTEST(nextion_frame, test_frames_follow_each_other)
{
    FrameParser parser;
    const uint8_t first[] = {0x01, 0x02, 0xFF, 0xFF, 0xFF};
    const uint8_t second[] = {0x03, 0xFF, 0xFF, 0xFF};
    zassert_equal(feedAll(parser, first, sizeof(first)), Event::Frame);
    zassert_equal(feedAll(parser, second, sizeof(second)), Event::Frame);
    zassert_equal(parser.len, 1);
    zassert_equal(parser.data[0], 0x03);
}

ZTEST(nextion_frame, test_full_size_frame)
{
    FrameParser parser;
    for (size_t i = 0; i < FrameParser::FRAME_MAX; i++) {
        zassert_equal(parser.feed(0x55), Event::None);
    }
    parser.feed(0xFF);
    parser.feed(0xFF);
    zassert_equal(parser.feed(0xFF), Event::Frame);
    zassert_equal(parser.len, FrameParser::FRAME_MAX);
}

ZTEST(nextion_frame, test_oversized_frame_is_dropped)
{
    FrameParser parser;
    for (size_t i = 0; i <= FrameParser::FRAME_MAX; i++) {
        zassert_equal(parser.feed(0x55), Event::None);
    }
    parser.feed(0xFF);
    parser.feed(0xFF);
    zassert_equal(parser.feed(0xFF), Event::Oversized);

    const uint8_t next[] = {0x09, 0xFF, 0xFF, 0xFF};
    zassert_equal(feedAll(parser, next, sizeof(next)), Event::Frame);
    zassert_equal(parser.len, 1);
}

ZTEST(nextion_frame, test_reset_drops_a_partial_frame)
{
    FrameParser parser;
    parser.feed(0x01);
    parser.feed(0xFF);
    parser.feed(0xFF);
    parser.reset();
    const uint8_t bytes[] = {0x02, 0xFF, 0xFF, 0xFF};
    zassert_equal(feedAll(parser, bytes, sizeof(bytes)), Event::Frame);
    zassert_equal(parser.len, 1);
    zassert_equal(parser.data[0], 0x02);
}

ZTEST_SUITE(nextion_attrs, NULL, NULL, resetAttrs, NULL, NULL);

ZTEST(nextion_attrs, test_writes_only_changes)
{
    zassert_true(Nextion::pushAttr("n0.val", "1"));
    zassert_str_equal(g_lastCommand, "n0.val=1");
    zassert_false(Nextion::pushAttr("n0.val", "1"));
    zassert_equal(g_commands, 1);
    zassert_true(Nextion::pushAttr("n0.val", "2"));
    zassert_true(Nextion::pushAttr("n1.val", "2"));
    zassert_equal(g_commands, 3);
}

ZTEST(nextion_attrs, test_num_text_color)
{
    zassert_true(Nextion::pushNum("n0.val", -7));
    zassert_str_equal(g_lastCommand, "n0.val=-7");
    zassert_true(Nextion::pushText("t0.txt", "%s %d", "pass", 3));
    zassert_str_equal(g_lastCommand, "t0.txt=\"pass 3\"");
    zassert_false(Nextion::pushText("t0.txt", "pass %d", 3));
    Nextion::pushColor("b0", "bco", 2178);
    zassert_str_equal(g_lastCommand, "b0.bco=2178");
}

ZTEST(nextion_attrs, test_text_has_no_quote_or_backslash)
{
    Nextion::pushText("t0.txt", "%s", "say \"hi\" \\ bye");
    zassert_str_equal(g_lastCommand, "t0.txt=\"say 'hi' / bye\"");
    zassert_equal(Nextion::safeChar('a'), 'a');
}

ZTEST(nextion_attrs, test_resync_pushes_everything_again)
{
    Nextion::pushAttr("n0.val", "1");
    Nextion::resync();
    zassert_true(Nextion::pushAttr("n0.val", "1"));
}

ZTEST(nextion_attrs, test_forget_page)
{
    Nextion::setPage(1);
    zassert_equal(Nextion::page(), 1);
    Nextion::pushAttr("a.val", "1");
    Nextion::setPage(2);
    Nextion::pushAttr("b.val", "1");
    Nextion::forgetPage(1);
    zassert_true(Nextion::pushAttr("a.val", "1"));
    zassert_false(Nextion::pushAttr("b.val", "1"));
}
