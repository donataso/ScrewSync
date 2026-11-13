import { describe, expect, it } from "vitest"
import type { Step, StepKind } from "./core"
import { markLathecodeTravel } from "./lathecode"

function step(kind: StepKind, line: number): Step {
  return { kind, zDu: 0, xDu: 0, feed: 1000, line }
}

describe("markLathecodeTravel", () => {
  const text = ["G1 Z-1 F100", "G1 X2 ; cut", "G1 Z0 ; CUT 2", "G1 X0 ; cutoff", "G32 Z-5 F1"].join("\n")

  it("turns feed moves without a cut comment into rapids", () => {
    const marked = markLathecodeTravel(text, [step("feed", 1), step("feed", 2), step("feed", 3), step("feed", 4)])
    expect(marked.map((s) => s.kind)).toEqual(["rapid", "feed", "feed", "rapid"])
  })

  it("keeps every other step kind", () => {
    const steps = [step("thread", 5), step("pause", 1), step("end", 1), step("rapid", 2)]
    expect(markLathecodeTravel(text, steps)).toEqual(steps)
  })

  it("keeps the step data and does not change its input", () => {
    const steps = [step("feed", 1)]
    const marked = markLathecodeTravel(text, steps)
    expect(marked[0]).toEqual({ ...steps[0], kind: "rapid" })
    expect(steps[0].kind).toBe("feed")
  })

  it("treats a step past the text as travel", () => {
    expect(markLathecodeTravel(text, [step("feed", 99)])[0].kind).toBe("rapid")
  })
})
