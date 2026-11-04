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
#include <string.h>
#include <stdlib.h>
#include "GcodeCore.hpp"
#include "GcodeLathecode.hpp"

using namespace Gcode;

namespace
{
    const unsigned char VECTORS_BLOB[] = {
#include "gcode_lathecode_vectors.inc"
    };
    constexpr size_t VECTORS_BLOB_LEN = sizeof(VECTORS_BLOB);
    constexpr size_t MAX_BLOB_CHARS = 32768;
    constexpr size_t MAX_LINES = 1024;
    constexpr size_t MAX_BLOCKS = 128;
    constexpr size_t MAX_VECTORS = 64;
    constexpr size_t MAX_STEPS_PER_VECTOR = 64;

    // Splits buf in place on sep, writing up to maxToks non-empty token
    // pointers into toks. Avoids strtok so it nests safely (line-splitting
    // calls this, then word-splitting calls it again on each line).
    // Unlike splitInPlace, keeps empty lines so line numbers stay right.
    size_t splitLines(char *buf, char **lines, size_t maxLines)
    {
        size_t count = 0;
        char *p = buf;
        while (*p) {
            zassert_true(count < maxLines, "too many lines");
            lines[count++] = p;
            while (*p && *p != '\n') {
                p++;
            }
            if (*p) {
                *p++ = '\0';
            }
        }
        return count;
    }

    size_t splitInPlace(char *buf, char sep, char **toks, size_t maxToks)
    {
        size_t count = 0;
        char *p = buf;
        while (*p) {
            while (*p == sep) {
                *p++ = '\0';
            }
            if (!*p) {
                break;
            }
            zassert_true(count < maxToks, "too many tokens");
            toks[count++] = p;
            while (*p && *p != sep) {
                p++;
            }
        }
        return count;
    }

    // One "=== <name>.<ext>" block found in the embedded bundle: the
    // lines[] range (within the top-level, newline-split array) holding
    // its body.
    struct Block
    {
        char name[64];
        bool isSteps;
        size_t firstLine;
        size_t lineCount;
    };

    bool stripSuffix(char *name, const char *suffix)
    {
        size_t nameLen = strlen(name);
        size_t suffixLen = strlen(suffix);
        if (nameLen < suffixLen) {
            return false;
        }
        if (strcmp(name + (nameLen - suffixLen), suffix) != 0) {
            return false;
        }
        name[nameLen - suffixLen] = '\0';
        return true;
    }

    // Scans the already newline-split bundle for "=== name.ext" headers
    // and records each block's body line range.
    size_t findBlocks(char **lines, size_t lineCount, Block *blocks, size_t maxBlocks)
    {
        size_t blockCount = 0;
        size_t i = 0;
        while (i < lineCount) {
            zassert_true(strncmp(lines[i], "=== ", 4) == 0, "expected a vector header line: %s", lines[i]);
            zassert_true(blockCount < maxBlocks, "too many vector blocks");

            Block &b = blocks[blockCount];
            char header[64];
            strncpy(header, lines[i] + 4, sizeof(header) - 1);
            header[sizeof(header) - 1] = '\0';

            if (stripSuffix(header, ".steps")) {
                b.isSteps = true;
            } else {
                zassert_true(stripSuffix(header, ".gcode"), "bad vector header: %s", lines[i]);
                b.isSteps = false;
            }
            strncpy(b.name, header, sizeof(b.name) - 1);
            b.name[sizeof(b.name) - 1] = '\0';

            i++;
            b.firstLine = i;
            while (i < lineCount && strncmp(lines[i], "=== ", 4) != 0) {
                i++;
            }
            b.lineCount = i - b.firstLine;
            blockCount++;
        }
        return blockCount;
    }

    struct Vector
    {
        const char *name;
        char **gcodeLines;
        size_t gcodeLineCount;
        char **stepsLines;
        size_t stepsLineCount;
    };

    // Pairs up .gcode/.steps blocks of the same name into vectors.
    size_t pairVectors(const Block *blocks, size_t blockCount, char **lines, Vector *vectors, size_t maxVectors)
    {
        size_t vectorCount = 0;
        for (size_t i = 0; i < blockCount; i++) {
            if (blocks[i].isSteps) {
                continue;
            }
            zassert_true(vectorCount < maxVectors, "too many vectors");
            Vector &v = vectors[vectorCount];
            v.name = blocks[i].name;
            v.gcodeLines = &lines[blocks[i].firstLine];
            v.gcodeLineCount = blocks[i].lineCount;
            v.stepsLines = nullptr;
            v.stepsLineCount = 0;

            for (size_t j = 0; j < blockCount; j++) {
                if (blocks[j].isSteps && strcmp(blocks[j].name, blocks[i].name) == 0) {
                    v.stepsLines = &lines[blocks[j].firstLine];
                    v.stepsLineCount = blocks[j].lineCount;
                    break;
                }
            }
            zassert_not_null(v.stepsLines, "%s: .gcode vector has no matching .steps", v.name);
            vectorCount++;
        }
        return vectorCount;
    }

    // Collects emitted steps for comparison against a vector's .steps file.
    class CollectSink : public StepSink
    {
    public:
        size_t count = 0;
        Step steps[MAX_STEPS_PER_VECTOR];

        void emit(const Step &step) override
        {
            zassert_true(count < MAX_STEPS_PER_VECTOR, "too many steps collected");
            steps[count++] = step;
        }
    };

    const char *kindName(StepKind kind)
    {
        switch (kind)
        {
        case StepKind::Rapid:
            return "rapid";
        case StepKind::Feed:
            return "feed";
        case StepKind::Thread:
            return "thread";
        case StepKind::Pause:
            return "pause";
        case StepKind::OptionalPause:
            return "optional-pause";
        case StepKind::End:
            return "end";
        }
        return "?";
    }

    bool isMotionKind(StepKind kind)
    {
        return kind == StepKind::Rapid || kind == StepKind::Feed || kind == StepKind::Thread;
    }

    bool hasFeedField(StepKind kind)
    {
        return kind == StepKind::Feed || kind == StepKind::Thread;
    }

    // One expectation parsed from a .steps line: either a step, or an
    // error at a given line number.
    struct Expectation
    {
        uint32_t line;
        bool isError;
        StepKind kind;
        int32_t zDu;
        int32_t xDu;
        int32_t feed;
    };

    bool parseKind(const char *text, StepKind &kind)
    {
        if (strcmp(text, "rapid") == 0) {
            kind = StepKind::Rapid;
            return true;
        }
        if (strcmp(text, "feed") == 0) {
            kind = StepKind::Feed;
            return true;
        }
        if (strcmp(text, "thread") == 0) {
            kind = StepKind::Thread;
            return true;
        }
        if (strcmp(text, "pause") == 0) {
            kind = StepKind::Pause;
            return true;
        }
        if (strcmp(text, "optional-pause") == 0) {
            kind = StepKind::OptionalPause;
            return true;
        }
        if (strcmp(text, "end") == 0) {
            kind = StepKind::End;
            return true;
        }
        return false;
    }

    // Parses a vector's already newline-split .steps lines into
    // expectations, skipping blank/comment lines.
    size_t parseSteps(char **lines, size_t lineCount, Expectation *out, size_t outCap)
    {
        size_t count = 0;
        for (size_t li = 0; li < lineCount; li++) {
            char *line = lines[li];
            while (*line == ' ' || *line == '\t') {
                line++;
            }
            if (*line == '\0' || *line == '#') {
                continue;
            }

            char *toks[8];
            size_t tokCount = splitInPlace(line, ' ', toks, 8);
            zassert_true(tokCount >= 2, "malformed .steps line: %s", line);

            zassert_true(count < outCap, "too many expectations");
            Expectation &e = out[count];
            e.isError = false;
            e.zDu = 0;
            e.xDu = 0;
            e.feed = 0;
            e.line = (uint32_t)strtoul(toks[0], nullptr, 10);

            if (strcmp(toks[1], "error") == 0) {
                e.isError = true;
            } else {
                zassert_true(parseKind(toks[1], e.kind), "unknown kind in .steps line: %s", line);
                bool haveZ = false, haveX = false, haveF = false;
                for (size_t ti = 2; ti < tokCount; ti++) {
                    if (strncmp(toks[ti], "z=", 2) == 0) {
                        e.zDu = atoi(toks[ti] + 2);
                        haveZ = true;
                    } else if (strncmp(toks[ti], "x=", 2) == 0) {
                        e.xDu = atoi(toks[ti] + 2);
                        haveX = true;
                    } else if (strncmp(toks[ti], "f=", 2) == 0) {
                        e.feed = atoi(toks[ti] + 2);
                        haveF = true;
                    }
                }
                zassert_true(!isMotionKind(e.kind) || (haveZ && haveX), "missing z/x in .steps line");
                zassert_true(!hasFeedField(e.kind) || haveF, "missing f in .steps line");
            }
            count++;
        }
        return count;
    }

    // Runs one vector's .gcode lines through the lathecode flavor and
    // checks the collected steps/error against its .steps expectations.
    void runVector(const Vector &v)
    {
        Expectation expectations[MAX_STEPS_PER_VECTOR];
        size_t expectCount = parseSteps(v.stepsLines, v.stepsLineCount, expectations, MAX_STEPS_PER_VECTOR);

        Lathecode flavor;
        flavor.begin(0, 0);
        CollectSink sink;

        const char *errorMsg = nullptr;
        uint32_t errorLine = 0;
        for (size_t i = 0; i < v.gcodeLineCount && !errorMsg; i++) {
            uint32_t lineNo = (uint32_t)(i + 1);
            const char *err = interpretLine(flavor, v.gcodeLines[i], lineNo, sink);
            if (err) {
                errorMsg = err;
                errorLine = lineNo;
            }
        }

        size_t stepIndex = 0;
        for (size_t i = 0; i < expectCount; i++) {
            const Expectation &e = expectations[i];
            if (e.isError) {
                zassert_not_null(errorMsg, "%s: expected error at line %u, got none", v.name, e.line);
                zassert_equal(errorLine, e.line, "%s: error at wrong line", v.name);
                zassert_equal(stepIndex, sink.count, "%s: steps emitted after/around the error", v.name);
                return;
            }
            zassert_true(stepIndex < sink.count, "%s: missing step at line %u", v.name, e.line);
            const Step &s = sink.steps[stepIndex];
            zassert_equal(s.line, e.line, "%s: step %u wrong line", v.name, (unsigned)stepIndex);
            zassert_equal((int)s.kind, (int)e.kind, "%s: step %u wrong kind (got %s)", v.name, (unsigned)stepIndex, kindName(s.kind));
            if (isMotionKind(e.kind)) {
                zassert_equal(s.zDu, e.zDu, "%s: step %u wrong zDu", v.name, (unsigned)stepIndex);
                zassert_equal(s.xDu, e.xDu, "%s: step %u wrong xDu", v.name, (unsigned)stepIndex);
            }
            if (hasFeedField(e.kind)) {
                zassert_equal(s.feed, e.feed, "%s: step %u wrong feed", v.name, (unsigned)stepIndex);
            }
            stepIndex++;
        }
        zassert_is_null(errorMsg, "%s: unexpected error: %s", v.name, errorMsg ? errorMsg : "");
        zassert_equal(stepIndex, sink.count, "%s: extra unexpected steps emitted", v.name);
    }
}

ZTEST_SUITE(gcode, NULL, NULL, NULL, NULL, NULL);

ZTEST(gcode, test_vectors)
{
    static char blob[MAX_BLOB_CHARS];
    zassert_true(VECTORS_BLOB_LEN < MAX_BLOB_CHARS, "vector bundle too large");
    memcpy(blob, VECTORS_BLOB, VECTORS_BLOB_LEN);
    blob[VECTORS_BLOB_LEN] = '\0';

    static char *lines[MAX_LINES];
    size_t lineCount = splitLines(blob, lines, MAX_LINES);

    static Block blocks[MAX_BLOCKS];
    size_t blockCount = findBlocks(lines, lineCount, blocks, MAX_BLOCKS);

    static Vector vectors[MAX_VECTORS];
    size_t vectorCount = pairVectors(blocks, blockCount, lines, vectors, MAX_VECTORS);
    zassert_true(vectorCount > 0, "no gcode test vectors found");

    for (size_t i = 0; i < vectorCount; i++) {
        runVector(vectors[i]);
    }
}

ZTEST(gcode, test_tokenize_strips_comments)
{
    Words words;
    zassert_is_null(tokenize("G1 X10 ; trailing comment", words));
    zassert_equal(words.count, 2);
    zassert_is_null(tokenize("G1 (inline) X10", words));
    zassert_equal(words.count, 2);
    zassert_equal(words.words[1].letter, 'X');
    zassert_is_null(tokenize("(only a comment)", words));
    zassert_equal(words.count, 0);
    zassert_is_null(tokenize("%", words));
    zassert_equal(words.count, 0);
}

ZTEST(gcode, test_tokenize_crlf)
{
    Words words;
    zassert_is_null(tokenize("G1 X10\r", words));
    zassert_equal(words.count, 2);
    zassert_equal(words.words[0].letter, 'G');
    zassert_equal(words.words[1].letter, 'X');
    zassert_within(words.words[1].value, 10.0, 1e-9);
}

ZTEST(gcode, test_tokenize_no_space_words)
{
    Words words;
    zassert_is_null(tokenize("G1X10Z-2.5F100", words));
    zassert_equal(words.count, 4);
    zassert_equal(words.words[0].letter, 'G');
    zassert_within(words.words[0].value, 1.0, 1e-9);
    zassert_equal(words.words[1].letter, 'X');
    zassert_within(words.words[1].value, 10.0, 1e-9);
    zassert_equal(words.words[2].letter, 'Z');
    zassert_within(words.words[2].value, -2.5, 1e-9);
    zassert_equal(words.words[3].letter, 'F');
    zassert_within(words.words[3].value, 100.0, 1e-9);
}

ZTEST(gcode, test_tokenize_n_word_ignored)
{
    Words words;
    zassert_is_null(tokenize("N100 G1 X10", words));
    zassert_equal(words.count, 2);
    zassert_equal(words.words[0].letter, 'G');
}

ZTEST(gcode, test_tokenize_malformed_words)
{
    Words words;
    zassert_not_null(tokenize("G1 X", words));
    zassert_not_null(tokenize("10 X5", words));
    zassert_not_null(tokenize("G1 #5", words));
}

ZTEST(gcode, test_tokenize_number_forms)
{
    Words words;

    zassert_is_null(tokenize("X1e3", words));
    zassert_equal(words.count, 2);
    zassert_equal(words.words[1].letter, 'E');
    zassert_is_null(tokenize("g0x10", words));
    zassert_equal(words.count, 2);
    zassert_equal(words.words[1].letter, 'X');
    zassert_not_null(tokenize("Xnan", words));
    zassert_not_null(tokenize("X-", words));

    zassert_is_null(tokenize("X.5", words));
    zassert_equal(words.count, 1);
    zassert_within(words.words[0].value, 0.5, 1e-9);

    zassert_is_null(tokenize("X-.5", words));
    zassert_equal(words.count, 1);
    zassert_within(words.words[0].value, -0.5, 1e-9);
}
