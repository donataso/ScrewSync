import { describe, expect, it } from "vitest"
import { detectDialect, find, list } from "./dialects"

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
})
