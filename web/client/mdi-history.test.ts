import { describe, expect, it } from "vitest"
import { addToHistory, freshRecall, HISTORY_MAX, stepHistory } from "./mdi-history"

describe("addToHistory", () => {
  it("skips empty lines and repeats of the last line", () => {
    let h = addToHistory([], "  ")
    h = addToHistory(h, "G0 Z1")
    h = addToHistory(h, "G0 Z1 ")
    h = addToHistory(h, "G0 Z2")
    expect(h).toEqual(["G0 Z1", "G0 Z2"])
  })

  it("keeps the newest lines only", () => {
    let h: string[] = []
    for (let i = 0; i < HISTORY_MAX + 5; i++) {
      h = addToHistory(h, `G0 Z${i}`)
    }
    expect(h.length).toBe(HISTORY_MAX)
    expect(h[h.length - 1]).toBe(`G0 Z${HISTORY_MAX + 4}`)
  })
})

describe("stepHistory", () => {
  const history = ["G90", "G0 Z1", "G0 Z2"]

  it("walks back, stops at the oldest, and returns to the draft", () => {
    let r = freshRecall(history)
    let step = stepHistory(history, r, "G1", -1)
    expect(step.text).toBe("G0 Z2")
    r = step.recall
    for (let i = 0; i < 5; i++) {
      step = stepHistory(history, r, step.text, -1)
      r = step.recall
    }
    expect(step.text).toBe("G90")
    for (let i = 0; i < 5; i++) {
      step = stepHistory(history, r, step.text, 1)
      r = step.recall
    }
    expect(step.text).toBe("G1")
  })

  it("does nothing with an empty history", () => {
    const step = stepHistory([], freshRecall([]), "G1", -1)
    expect(step.text).toBe("G1")
  })
})
