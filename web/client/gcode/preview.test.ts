import { describe, expect, it } from "vitest"
import { DU_PER_MM, type Step, type StepKind } from "./core"
import { CANVAS_H, CANVAS_W, computeMapping } from "./preview"

function step(kind: StepKind, zMm: number, xMm: number): Step {
  return { kind, zDu: zMm * DU_PER_MM, xDu: xMm * DU_PER_MM, feed: 0, line: 1 }
}

describe("computeMapping", () => {
  it("puts the start at the canvas centre when nothing moves", () => {
    const m = computeMapping([])
    expect(m.toPx(0, 0)).toEqual([CANVAS_W / 2, CANVAS_H / 2])
    expect(m.pxPerMm).toBeGreaterThan(0)
  })

  it("includes the start point in the bounds", () => {
    const m = computeMapping([step("feed", -20, 5)])
    expect([m.zMinMm, m.zMaxMm, m.xMinMm, m.xMaxMm]).toEqual([-20, 0, 0, 5])
  })

  it("ignores steps without motion", () => {
    const m = computeMapping([step("feed", -10, 2), step("pause", -500, 500), step("end", 900, -900)])
    expect([m.zMinMm, m.zMaxMm, m.xMinMm, m.xMaxMm]).toEqual([-10, 0, 0, 2])
  })

  it("fits every target inside the canvas at one scale", () => {
    const steps = [step("rapid", -40, 10), step("feed", -40, -3), step("thread", 15, -3)]
    const m = computeMapping(steps)
    for (const [zMm, xMm] of [[0, 0], [-40, 10], [-40, -3], [15, -3]] as const) {
      const [px, py] = m.toPx(zMm, xMm)
      expect(px).toBeGreaterThanOrEqual(0)
      expect(px).toBeLessThanOrEqual(CANVAS_W)
      expect(py).toBeGreaterThanOrEqual(0)
      expect(py).toBeLessThanOrEqual(CANVAS_H)
    }
    const [x0, y0] = m.toPx(0, 0)
    const [x1, y1] = m.toPx(1, 1)
    expect(x1 - x0).toBeCloseTo(m.pxPerMm)
    expect(y1 - y0).toBeCloseTo(m.pxPerMm)
  })

  it("runs Z left to right and X top to bottom", () => {
    const m = computeMapping([step("feed", -10, 5)])
    expect(m.toPx(-10, 0)[0]).toBeLessThan(m.toPx(0, 0)[0])
    expect(m.toPx(0, 5)[1]).toBeGreaterThan(m.toPx(0, 0)[1])
  })

  it("exposes the origin of the same mapping", () => {
    const m = computeMapping([step("feed", -30, 8)])
    const [px, py] = m.toPx(-12, 3)
    expect(m.originX + -12 * m.pxPerMm).toBeCloseTo(px)
    expect(m.originY + 3 * m.pxPerMm).toBeCloseTo(py)
  })
})
