// Standard (LinuxCNC) interpreter: G0/G1/G33, G20/G21, G90/G91, G94/G18
// (no-op), M0/M1/M2/M30, F, K, T (ignored). Z/X axes only, X is radius.
// Mirrors main/lib/Gcode/GcodeInterpreter.{hpp,cpp}.

import { DU_PER_INCH, DU_PER_MM, type StepKind, type StepSink, type Word } from "./core"

const INT32_MAX = 2147483647
const INT32_MIN = -2147483648

// Converts a word's raw value (in current units) to du, rejecting anything
// that would overflow int32.
function scaledValueToDu(value: number, unitScaleDu: number): number | null {
  const scaled = value * unitScaleDu
  if (!Number.isFinite(scaled) || scaled > INT32_MAX || scaled < INT32_MIN) {
return null
}
  // Half away from zero, like the firmware's llround.
  return Math.sign(scaled) * Math.round(Math.abs(scaled))
}

function axisTargetDu(value: number, currentDu: number, absolute: boolean, unitScaleDu: number): number | null {
  const rawDu = scaledValueToDu(value, unitScaleDu)
  if (rawDu === null) {
return null
}
  const target = absolute ? rawDu : currentDu + rawDu
  if (target < INT32_MIN || target > INT32_MAX) {
return null
}
  return target
}

// G/M numbers in tenths (G38.2 = 382); anything finer is refused.
function exactTenths(value: number): number | null {
  if (!(value >= 0 && value <= 1000)) {
    return null
  }
  const tenths = value * 10
  const rounded = Math.round(tenths)
  if (Math.abs(tenths - rounded) > 1e-6) {
    return null
  }
  return rounded
}

export enum Motion {
  None,
  Rapid,
  Feed,
  Thread,
}

export class Interpreter {
  private motion: Motion = Motion.None
  private absolute = true
  private unitScaleDu: number = DU_PER_MM
  private feedDuPerMin = 0
  private feedSet = false
  private zDu = 0
  private xDu = 0

  begin(startZDu: number, startXDu: number, motion: Motion = Motion.None): void {
    this.motion = motion
    this.absolute = true
    this.unitScaleDu = DU_PER_MM
    this.feedDuPerMin = 0
    this.feedSet = false
    this.zDu = startZDu
    this.xDu = startXDu
  }

  line(words: Word[], lineNo: number, sink: StepSink): string | null {
    let hasF = false
    let hasK = false
    let hasZ = false
    let hasX = false
    let hasM = false
    let hasG33 = false
    let fVal = 0
    let kVal = 0
    let zVal = 0
    let xVal = 0
    let mKind: StepKind = "pause"

    for (const w of words) {
      switch (w.letter) {
        case "G": {
          const code = exactTenths(w.value)
          switch (code) {
            case 0:
              this.motion = Motion.Rapid
              break
            case 10:
              this.motion = Motion.Feed
              break
            case 330:
              this.motion = Motion.Thread
              hasG33 = true
              break
            case 200:
              this.unitScaleDu = DU_PER_INCH
              break
            case 210:
              this.unitScaleDu = DU_PER_MM
              break
            case 900:
              this.absolute = true
              break
            case 910:
              this.absolute = false
              break
            case 940:
            case 180:
              break
            default:
              return "unsupported command"
          }
          break
        }
        case "M": {
          const code = exactTenths(w.value)
          switch (code) {
            case 0:
              mKind = "pause"
              break
            case 10:
              mKind = "optional-pause"
              break
            case 20:
            case 300:
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
          if (hasF) {
            return "duplicate word"
          }
          hasF = true
          fVal = w.value
          break
        case "K":
          if (hasK) {
            return "duplicate word"
          }
          hasK = true
          kVal = w.value
          break
        case "Z":
          if (hasZ) {
            return "duplicate word"
          }
          hasZ = true
          zVal = w.value
          break
        case "X":
          if (hasX) {
            return "duplicate word"
          }
          hasX = true
          xVal = w.value
          break
        default:
          return "unsupported axis"
      }
    }

    if (hasG33 && !hasZ && !hasX) {
      return "G33 requires axis move"
    }
    if (hasK && this.motion !== Motion.Thread) {
      return "K requires G33"
    }

    if (hasF) {
      const fDu = fVal > 0 ? scaledValueToDu(fVal, this.unitScaleDu) : null
      if (fDu === null || fDu <= 0) {
        return "value out of range"
      }
      this.feedDuPerMin = fDu
      this.feedSet = true
    }

    let leadDu = 0
    if (hasK) {
      const k = kVal > 0 ? scaledValueToDu(kVal, this.unitScaleDu) : null
      if (k === null || k <= 0) {
        return "value out of range"
      }
      leadDu = k
    }

    if (hasZ || hasX) {
      if (this.motion === Motion.None) {
        return "no motion mode"
      }
      let newZDu = this.zDu
      let newXDu = this.xDu
      if (hasZ) {
        const z = axisTargetDu(zVal, this.zDu, this.absolute, this.unitScaleDu)
        if (z === null) {
          return "value out of range"
        }
        newZDu = z
      }
      if (hasX) {
        const x = axisTargetDu(xVal, this.xDu, this.absolute, this.unitScaleDu)
        if (x === null) {
          return "value out of range"
        }
        newXDu = x
      }

      if (newZDu !== this.zDu || newXDu !== this.xDu) {
        let kind: StepKind
        let feed: number
        if (this.motion === Motion.Rapid) {
          kind = "rapid"
          feed = 0
        } else if (this.motion === Motion.Feed) {
          if (!this.feedSet) {
            return "no feed rate"
          }
          kind = "feed"
          feed = this.feedDuPerMin
        } else {
          if (!hasK) {
            return "G33 requires K"
          }
          kind = "thread"
          feed = leadDu
        }
        this.zDu = newZDu
        this.xDu = newXDu
        sink.emit({ kind, zDu: newZDu, xDu: newXDu, feed, line: lineNo })
      } else if (this.motion === Motion.Thread) {
        return "G33 requires axis move"
      }
    }

    if (hasM) {
      sink.emit({ kind: mKind, zDu: this.zDu, xDu: this.xDu, feed: 0, line: lineNo })
    }

    return null
  }
}
