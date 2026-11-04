import { describe, expect, it } from "vitest"
import { detectFlavor, find, list } from "./flavors"

describe("flavors registry", () => {
  it("finds lathecode by id", () => {
    expect(find("lathecode")?.id()).toBe("lathecode")
  })

  it("returns null for an unknown id", () => {
    expect(find("nope")).toBeNull()
  })

  it("lists at least the lathecode flavor", () => {
    expect(list().map((f) => f.id())).toContain("lathecode")
  })
})

describe("detectFlavor", () => {
  it("detects lathecode generator output by its header", () => {
    const text = [
      "; Cone 20x10x30",
      "; Run time 1.2 min, cutting 85% of time",
      "; Axes expected: Z+ points right, X+ points up",
      "G21",
    ].join("\n")
    expect(detectFlavor(text)).toBe("lathecode")
  })

  it("detects lathecode by the zero-tool comment", () => {
    expect(detectFlavor("X20 ; zero your tool X on centerline")).toBe("lathecode")
  })

  it("returns null for unrecognized text", () => {
    expect(detectFlavor("G1 X1 Z1")).toBeNull()
  })
})
