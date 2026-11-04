// Flavor for https://kachurovskiy.com/lathecode/ output: G0/G1/G32,
// G20/G21, G90/G91, G94/G18 (no-op), M0/M1/M2/M30, F, T. Z/X axes only,
// X is radius from the centreline. Mirrors
// main/lib/Gcode/GcodeLathecode.{hpp,cpp}.

import { DU_PER_INCH, DU_PER_MM, type Flavor, type Step, type StepKind, type StepSink, type Word } from "./core"

const INT32_MAX = 2147483647
const INT32_MIN = -2147483648

// Converts a word's raw value (in current units) to du, rejecting anything
// that would overflow int32.
function scaledValueToDu(value: number, unitScaleDu: number): number | null {
  const scaled = value * unitScaleDu
  if (!Number.isFinite(scaled) || scaled > INT32_MAX || scaled < INT32_MIN) return null
  // Half away from zero, like the firmware's llround.
  return Math.sign(scaled) * Math.round(Math.abs(scaled))
}

// Applies the modal unit scale/positioning mode to one axis word.
function axisTargetDu(value: number, currentDu: number, absolute: boolean, unitScaleDu: number): number | null {
  const rawDu = scaledValueToDu(value, unitScaleDu)
  if (rawDu === null) return null
  const target = absolute ? rawDu : currentDu + rawDu
  if (target < INT32_MIN || target > INT32_MAX) return null
  return target
}

// G/M codes must be exact integers; no rounding.
function exactInt(value: number): number | null {
  if (!(value >= 0 && value <= 1000)) return null
  const truncated = Math.trunc(value)
  if (truncated !== value) return null
  return truncated
}

enum Motion {
  Rapid,
  Feed,
  Thread,
}

export class Lathecode implements Flavor {
  private motion: Motion = Motion.Feed
  private absolute = true
  private unitScaleDu: number = DU_PER_MM
  private feedDuPerMin = 0
  private feedSet = false
  private threadLeadDu = 0
  private threadLeadSet = false
  private zDu = 0
  private xDu = 0

  id(): string {
    return "lathecode"
  }

  begin(startZDu: number, startXDu: number): void {
    this.motion = Motion.Feed
    this.absolute = true
    this.unitScaleDu = DU_PER_MM
    this.feedDuPerMin = 0
    this.feedSet = false
    this.threadLeadDu = 0
    this.threadLeadSet = false
    this.zDu = startZDu
    this.xDu = startXDu
  }

  line(words: Word[], lineNo: number, sink: StepSink): string | null {
    let hasF = false
    let hasZ = false
    let hasX = false
    let hasM = false
    let hasG32 = false
    let fVal = 0
    let zVal = 0
    let xVal = 0
    let mKind: StepKind = "pause"

    for (const w of words) {
      switch (w.letter) {
        case "G": {
          const g = exactInt(w.value)
          if (g === null) return "unsupported command"
          switch (g) {
            case 0:
              this.motion = Motion.Rapid
              break
            case 1:
              this.motion = Motion.Feed
              break
            case 32:
              this.motion = Motion.Thread
              hasG32 = true
              break
            case 20:
              this.unitScaleDu = DU_PER_INCH
              break
            case 21:
              this.unitScaleDu = DU_PER_MM
              break
            case 90:
              this.absolute = true
              break
            case 91:
              this.absolute = false
              break
            case 94:
            case 18:
              break
            default:
              return "unsupported command"
          }
          break
        }
        case "M": {
          const m = exactInt(w.value)
          if (m === null) return "unsupported command"
          switch (m) {
            case 0:
              mKind = "pause"
              break
            case 1:
              mKind = "optional-pause"
              break
            case 2:
            case 30:
              mKind = "end"
              break
            default:
              return "unsupported command"
          }
          hasM = true
          break
        }
        case "T":
          break
        case "F":
          if (hasF) return "duplicate word"
          hasF = true
          fVal = w.value
          break
        case "Z":
          if (hasZ) return "duplicate word"
          hasZ = true
          zVal = w.value
          break
        case "X":
          if (hasX) return "duplicate word"
          hasX = true
          xVal = w.value
          break
        default:
          return "unsupported axis"
      }
    }

    if (hasG32 && !hasZ && !hasX) return "G32 requires axis move"

    // G32's F is the thread lead for this move, not the modal feed.
    const threadMove = this.motion === Motion.Thread
    if (hasF) {
      if (fVal <= 0) return "value out of range"
      const fDu = scaledValueToDu(fVal, this.unitScaleDu)
      if (fDu === null || fDu <= 0) return "value out of range"
      if (threadMove) {
        this.threadLeadDu = fDu
        this.threadLeadSet = true
      } else {
        this.feedDuPerMin = fDu
        this.feedSet = true
      }
    }

    if (hasZ || hasX) {
      let newZDu = this.zDu
      let newXDu = this.xDu
      if (hasZ) {
        const z = axisTargetDu(zVal, this.zDu, this.absolute, this.unitScaleDu)
        if (z === null) return "value out of range"
        newZDu = z
      }
      if (hasX) {
        const x = axisTargetDu(xVal, this.xDu, this.absolute, this.unitScaleDu)
        if (x === null) return "value out of range"
        newXDu = x
      }

      if (newZDu !== this.zDu || newXDu !== this.xDu) {
        let kind: StepKind
        let feed: number
        if (this.motion === Motion.Rapid) {
          kind = "rapid"
          feed = 0
        } else if (this.motion === Motion.Feed) {
          if (!this.feedSet) return "no feed rate"
          kind = "feed"
          feed = this.feedDuPerMin
        } else {
          if (!this.threadLeadSet) return "no feed rate"
          kind = "thread"
          feed = this.threadLeadDu
        }
        this.zDu = newZDu
        this.xDu = newXDu
        sink.emit({ kind, zDu: newZDu, xDu: newXDu, feed, line: lineNo })
      } else if (threadMove) {
        return "G32 requires axis move"
      }
    }

    if (hasM) {
      const step: Step = { kind: mKind, zDu: this.zDu, xDu: this.xDu, feed: 0, line: lineNo }
      sink.emit(step)
    }

    return null
  }
}

// True if text looks like output from the kachurovskiy/lathecode generator.
export function detectLathecode(text: string): boolean {
  return /axes expected:/i.test(text) || /zero your tool x on centerline/i.test(text)
}

// LatheCode has no G0: its non-cutting moves are the feed moves without a "; cut" comment.
export function markLathecodeTravel(text: string, steps: Step[]): Step[] {
  const lines = text.split("\n")
  return steps.map((s) =>
    s.kind === "feed" && !/;\s*cut\b/i.test(lines[s.line - 1] ?? "") ? { ...s, kind: "rapid" } : s,
  )
}
