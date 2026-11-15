import { describe, expect, it } from "vitest"
import { parseProbe } from "./probe"

describe("parseProbe", () => {
  it("reads a result", () => {
    expect(parseProbe("x found=1 pos=-1250 motor=40031")).toEqual({ axis: "X", found: true, posDu: -1250, motorDu: 40031 })
    expect(parseProbe("z found=0 pos=0 motor=0")?.found).toBe(false)
  })

  it("refuses other text", () => {
    expect(parseProbe("z found=1 pos=1")).toBeNull()
    expect(parseProbe("")).toBeNull()
  })
})
