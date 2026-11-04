import { describe, expect, it } from "vitest"
import { tokenize } from "./core"

describe("tokenize", () => {
  it("strips ';' and '(...)' comments", () => {
    expect(tokenize("G1 X10 ; trailing comment")).toEqual({ error: null, words: [{ letter: "G", value: 1 }, { letter: "X", value: 10 }] })
    const inline = tokenize("G1 (inline) X10")
    expect(inline.error).toBeNull()
    expect(inline.words).toHaveLength(2)
    expect(inline.words[1]).toEqual({ letter: "X", value: 10 })
  })

  it("treats a comment-only or empty line as no words", () => {
    expect(tokenize("(only a comment)")).toEqual({ error: null, words: [] })
    expect(tokenize("")).toEqual({ error: null, words: [] })
  })

  it("ignores a lone '%' line", () => {
    expect(tokenize("%")).toEqual({ error: null, words: [] })
    expect(tokenize("  %  ")).toEqual({ error: null, words: [] })
  })

  it("errors on a '%' followed by other content", () => {
    expect(tokenize("%G1").error).toBe("malformed word")
  })

  it("drops N<num> words", () => {
    const r = tokenize("N10 G1 X5")
    expect(r.error).toBeNull()
    expect(r.words).toEqual([{ letter: "G", value: 1 }, { letter: "X", value: 5 }])
  })

  it("treats \\r as whitespace", () => {
    const r = tokenize("G1\rX5\r")
    expect(r.error).toBeNull()
    expect(r.words).toEqual([{ letter: "G", value: 1 }, { letter: "X", value: 5 }])
  })

  it("is case-insensitive on letters", () => {
    const r = tokenize("g1 x5")
    expect(r.error).toBeNull()
    expect(r.words).toEqual([{ letter: "G", value: 1 }, { letter: "X", value: 5 }])
  })

  it("rejects malformed words", () => {
    expect(tokenize("G1 X").error).toBe("malformed word")
    expect(tokenize("1G5").error).toBe("malformed word")
  })

  it("rejects lines that are too long", () => {
    expect(tokenize("G1 X" + "1".repeat(200)).error).toBe("line too long")
  })

  it("rejects too many words", () => {
    const words = Array.from({ length: 20 }, (_, i) => `X${i}`).join(" ")
    expect(tokenize(words).error).toBe("too many words")
  })

  it("parses plain decimals with sign", () => {
    const r = tokenize("X-1.5 Y+2.25")
    expect(r.error).toBeNull()
    expect(r.words[0]).toEqual({ letter: "X", value: -1.5 })
    expect(r.words[1]).toEqual({ letter: "Y", value: 2.25 })
  })
})
