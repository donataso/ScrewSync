import { describe, expect, it } from "vitest"
import { hasXWord, unavailableReason, usesProbe } from "./needs"

describe("hasXWord", () => {
  it("finds an X word", () => {
    expect(hasXWord("G0 X5 Z2")).toBe(true)
    expect(hasXWord("g1 x-1.5")).toBe(true)
    expect(hasXWord("G1 X#1")).toBe(true)
    expect(hasXWord("G1 X[#1+1]")).toBe(true)
  })

  it("ignores comments, names and other words", () => {
    expect(hasXWord("G0 Z5 (X5)")).toBe(false)
    expect(hasXWord("G0 Z5 ; X5")).toBe(false)
    expect(hasXWord("#<x> = 5\nG0 Z[#<x>]")).toBe(false)
    expect(hasXWord("o<xprobe> call [1]")).toBe(false)
    expect(hasXWord("M3 S100")).toBe(false)
  })
})

describe("usesProbe", () => {
  it("finds G38 moves", () => {
    expect(usesProbe("G38.2 Z-5 F10")).toBe(true)
    expect(usesProbe("G0 Z5")).toBe(false)
    expect(usesProbe("G380 Z5")).toBe(false)
  })
})

describe("unavailableReason", () => {
  const on = { has_x_motor: true, has_probe: true }
  it("passes when the capabilities are unknown or present", () => {
    expect(unavailableReason("G0 X5", "linuxcnc", null)).toBeNull()
    expect(unavailableReason("G0 X5\nG38.2 Z-1 F5", "linuxcnc", on)).toBeNull()
  })

  it("refuses an X word without the X motor", () => {
    expect(unavailableReason("G0 X5", "linuxcnc", { ...on, has_x_motor: false })).toContain("X motor")
    expect(unavailableReason("G0 Z5", "linuxcnc", { ...on, has_x_motor: false })).toBeNull()
  })

  it("allows an X word in a routine", () => {
    expect(unavailableReason("o<r> sub\nG0 X5\no<r> endsub", "ngc", { ...on, has_x_motor: false })).toBeNull()
  })

  it("refuses G38 without the probe", () => {
    expect(unavailableReason("G38.2 Z-1 F5", "linuxcnc", { ...on, has_probe: false })).toContain("probe")
  })
})
