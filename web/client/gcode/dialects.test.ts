import { describe, expect, it } from "vitest"
import { detectDialect, find, list, usesNgc } from "./dialects"

describe("dialects registry", () => {
  it("finds lathecode by id", () => {
    expect(find("lathecode")?.id()).toBe("lathecode")
  })

  it("returns null for an unknown id", () => {
    expect(find("nope")).toBeNull()
  })

  it("lists the lathecode and linuxcnc dialects", () => {
    expect(list().map((d) => d.id())).toEqual(expect.arrayContaining(["lathecode", "linuxcnc"]))
  })
})

describe("detectDialect", () => {
  it("detects lathecode generator output by its header", () => {
    const text = [
      "; Cone 20x10x30",
      "; Run time 1.2 min, cutting 85% of time",
      "; Axes expected: Z+ points right, X+ points up",
      "G21",
    ].join("\n")
    expect(detectDialect(text)).toBe("lathecode")
  })

  it("detects lathecode by the zero-tool comment", () => {
    expect(detectDialect("X20 ; zero your tool X on centerline")).toBe("lathecode")
  })

  it("falls back to linuxcnc", () => {
    expect(detectDialect("G1 X1 Z1")).toBe("linuxcnc")
  })

  it("detects a routine by its sub", () => {
    expect(detectDialect("O<probe> sub\nG0 X1\no<probe> endsub")).toBe("ngc")
    expect(detectDialect("(o<probe> sub)\nG0 X1")).toBe("linuxcnc")
  })
})

describe("usesNgc", () => {
  it("finds NGC syntax outside comments", () => {
    expect(usesNgc("G0 X#1")).toBe(true)
    expect(usesNgc("G0 X[1+2]")).toBe(true)
    expect(usesNgc("N10 o100 if [1]")).toBe(true)
    expect(usesNgc("G38.2 Z-1 F10")).toBe(true)
    expect(usesNgc("G0 X1 (#1 [x])\nG1 Z2 ; #2")).toBe(false)
  })
})
