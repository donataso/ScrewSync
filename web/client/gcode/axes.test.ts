import { describe, expect, it } from "vitest"
import { axesMismatched, flipMismatchedAxes, parseAxesHeader } from "./axes"

describe("parseAxesHeader", () => {
  it("parses the generator's header", () => {
    const h = parseAxesHeader("; Axes expected: Z+ points right, X+ points up\nG0 X0\n")
    expect(h).toMatchObject({ zDir: "right", xDir: "up" })
  })

  it("is case-insensitive and tolerant of spacing", () => {
    const h = parseAxesHeader("; AXES EXPECTED:Z+points left,X+points down\n")
    expect(h).toMatchObject({ zDir: "left", xDir: "down" })
  })

  it("returns null when there's no header", () => {
    expect(parseAxesHeader("G0 X0 Z0\n")).toBeNull()
  })
})

describe("axesMismatched", () => {
  it("matches only right+down", () => {
    expect(axesMismatched({ zDir: "right", xDir: "down", start: 0, lineEnd: 0 })).toBe(false)
    expect(axesMismatched({ zDir: "right", xDir: "up", start: 0, lineEnd: 0 })).toBe(true)
    expect(axesMismatched({ zDir: "left", xDir: "down", start: 0, lineEnd: 0 })).toBe(true)
    expect(axesMismatched({ zDir: "left", xDir: "up", start: 0, lineEnd: 0 })).toBe(true)
  })
})

describe("flipMismatchedAxes", () => {
  it("leaves text unchanged when there's no header", () => {
    const text = "G0 X20 Z-5\n"
    expect(flipMismatchedAxes(text)).toBe(text)
  })

  it("leaves text unchanged when already right, down", () => {
    const text = "; Axes expected: Z+ points right, X+ points down\nG0 X20 Z-5\n"
    expect(flipMismatchedAxes(text)).toBe(text)
  })

  it("flips only X when X+ points up", () => {
    const text = [
      "; Axes expected: Z+ points right, X+ points up",
      "G0 X20 Z-5",
      "G1 X-0.1 Z10 F100",
      "G91",
      "X0 Z0",
    ].join("\n")
    const flipped = flipMismatchedAxes(text)
    expect(flipped).toBe(
      ["; Axes expected: Z+ points right, X+ points down", "G0 X-20 Z-5", "G1 X0.1 Z10 F100", "G91", "X0 Z0"].join(
        "\n",
      ),
    )
  })

  it("flips only Z when Z+ points left", () => {
    const text = "; Axes expected: Z+ points left, X+ points down\nG0 X20 Z-5\n"
    expect(flipMismatchedAxes(text)).toBe("; Axes expected: Z+ points right, X+ points down\nG0 X20 Z5\n")
  })

  it("flips both when Z+ points left, X+ points up", () => {
    const text = "; Axes expected: Z+ points left, X+ points up\nG0 X20 Z-5\n"
    expect(flipMismatchedAxes(text)).toBe("; Axes expected: Z+ points right, X+ points down\nG0 X-20 Z5\n")
  })

  it("does not touch Z/X inside ';' or '(...)' comments", () => {
    const text = [
      "; Axes expected: Z+ points right, X+ points up",
      "G0 X20 Z-5 ; zero your tool X on centerline, X10 Z10",
      "G1 X10 (comment with X5 Z5) Z1 F100",
    ].join("\n")
    const flipped = flipMismatchedAxes(text)
    expect(flipped).toBe(
      [
        "; Axes expected: Z+ points right, X+ points down",
        "G0 X-20 Z-5 ; zero your tool X on centerline, X10 Z10",
        "G1 X-10 (comment with X5 Z5) Z1 F100",
      ].join("\n"),
    )
  })

  it("keeps X0 as X0, not -X0 or X-0", () => {
    const text = "; Axes expected: Z+ points right, X+ points up\nG0 X0 Z0\n"
    expect(flipMismatchedAxes(text)).toBe("; Axes expected: Z+ points right, X+ points down\nG0 X0 Z0\n")
  })
})
