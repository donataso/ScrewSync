// Runs every tests/gcode/lathecode/*.gcode vector against the TS Lathecode
// flavor and checks it against the matching .steps file, the same way
// main/tests/unit/src/test_gcode.cpp checks the C++ flavor.

import { readdirSync, readFileSync } from "node:fs"
import { dirname, join } from "node:path"
import { fileURLToPath } from "node:url"
import { describe, expect, it } from "vitest"
import { interpretLine, type Step, type StepKind, type StepSink } from "./core"
import { Lathecode } from "./lathecode"

const HERE = dirname(fileURLToPath(import.meta.url))
const VECTORS_DIR = join(HERE, "..", "..", "tests", "gcode", "lathecode")

interface Expectation {
  line: number
  isError: boolean
  kind?: StepKind
  zDu?: number
  xDu?: number
  feed?: number
}

function isMotionKind(kind: StepKind): boolean {
  return kind === "rapid" || kind === "feed" || kind === "thread"
}

function hasFeedField(kind: StepKind): boolean {
  return kind === "feed" || kind === "thread"
}

function parseSteps(text: string): Expectation[] {
  const out: Expectation[] = []
  for (const raw of text.split("\n")) {
    const line = raw.trim()
    if (!line || line.startsWith("#")) continue
    const toks = line.split(/\s+/)
    const lineNo = parseInt(toks[0], 10)
    if (toks[1] === "error") {
      out.push({ line: lineNo, isError: true })
      continue
    }
    const kind = toks[1] as StepKind
    const e: Expectation = { line: lineNo, isError: false, kind }
    for (const tok of toks.slice(2)) {
      if (tok.startsWith("z=")) e.zDu = parseInt(tok.slice(2), 10)
      else if (tok.startsWith("x=")) e.xDu = parseInt(tok.slice(2), 10)
      else if (tok.startsWith("f=")) e.feed = parseInt(tok.slice(2), 10)
    }
    out.push(e)
  }
  return out
}

class CollectSink implements StepSink {
  steps: Step[] = []
  emit(step: Step): void {
    this.steps.push(step)
  }
}

function runVector(gcodeText: string): { steps: Step[]; errorMsg: string | null; errorLine: number } {
  const flavor = new Lathecode()
  flavor.begin(0, 0)
  const sink = new CollectSink()

  const lines = gcodeText.split("\n")
  let errorMsg: string | null = null
  let errorLine = 0
  for (let i = 0; i < lines.length && !errorMsg; i++) {
    const lineNo = i + 1
    const err = interpretLine(flavor, lines[i], lineNo, sink)
    if (err) {
      errorMsg = err
      errorLine = lineNo
    }
  }
  return { steps: sink.steps, errorMsg, errorLine }
}

const names = readdirSync(VECTORS_DIR)
  .filter((f) => f.endsWith(".gcode"))
  .map((f) => f.slice(0, -".gcode".length))
  .sort()

describe("lathecode vectors", () => {
  it("found test vectors", () => {
    expect(names.length).toBeGreaterThan(0)
  })

  for (const name of names) {
    it(name, () => {
      const gcodeText = readFileSync(join(VECTORS_DIR, `${name}.gcode`), "utf-8")
      const stepsText = readFileSync(join(VECTORS_DIR, `${name}.steps`), "utf-8")
      const expectations = parseSteps(stepsText)
      const { steps, errorMsg, errorLine } = runVector(gcodeText)

      let stepIndex = 0
      for (const e of expectations) {
        if (e.isError) {
          expect(errorMsg, `expected error at line ${e.line}, got none`).not.toBeNull()
          expect(errorLine).toBe(e.line)
          expect(stepIndex).toBe(steps.length)
          return
        }
        expect(stepIndex < steps.length, `missing step at line ${e.line}`).toBe(true)
        const s = steps[stepIndex]
        expect(s.line).toBe(e.line)
        expect(s.kind).toBe(e.kind)
        if (e.kind && isMotionKind(e.kind)) {
          expect(s.zDu).toBe(e.zDu)
          expect(s.xDu).toBe(e.xDu)
        }
        if (e.kind && hasFeedField(e.kind)) {
          expect(s.feed).toBe(e.feed)
        }
        stepIndex++
      }
      expect(errorMsg).toBeNull()
      expect(stepIndex).toBe(steps.length)
    })
  }
})
