// Standard (LinuxCNC) interpreter: G0/G1/G33, G10 L20 P1, G20/G21, G90/G91,
// G94/G18/G8/G54 (no-op), M0/M1/M2/M30, F, K, T (ignored). Z/X axes only, X
// is radius. Mirrors main/lib/Gcode/GcodeInterpreter.{hpp,cpp}, which also
// takes M100/M110 in MDI.

import { AXIS_X, AXIS_Z, DU_PER_INCH, DU_PER_MM, type StepKind, type StepSink, type Word } from "./core"

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
    const has: Record<string, boolean> = {}
    const val: Record<string, number> = {}
    let hasM = false
    let hasG33 = false
    let hasMotionG = false
    let hasG10 = false
    let mKind: StepKind = "pause"

    for (const w of words) {
      switch (w.letter) {
        case "G": {
          const code = exactTenths(w.value)
          switch (code) {
            case 0:
              if (hasMotionG) {
                return "two motion commands"
              }
              this.motion = Motion.Rapid
              hasMotionG = true
              break
            case 10:
              if (hasMotionG) {
                return "two motion commands"
              }
              this.motion = Motion.Feed
              hasMotionG = true
              break
            case 330:
              if (hasMotionG) {
                return "two motion commands"
              }
              this.motion = Motion.Thread
              hasMotionG = true
              hasG33 = true
              break
            case 382:
            case 383:
            case 384:
            case 385:
              return "only in MDI"
            case 100:
              hasG10 = true
              break
            case 70:
              return "G7 not supported, X is radius"
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
            case 80:
            case 540:
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
            case 1000:
            case 1100:
              return "only in MDI"
            default:
              return "unsupported command"
          }
          hasM = true
          break
        }
        case "T":
          break
        case "F":
        case "K":
        case "Z":
        case "X":
        case "L":
        case "P":
        case "R":
          if (has[w.letter]) {
            return "duplicate word"
          }
          has[w.letter] = true
          val[w.letter] = w.value
          break
        default:
          return "unsupported axis"
      }
    }

    const hasF = has.F === true
    const hasK = has.K === true
    const hasZ = has.Z === true
    const hasX = has.X === true
    const fVal = val.F ?? 0
    const kVal = val.K ?? 0
    const zVal = val.Z ?? 0
    const xVal = val.X ?? 0

    if ((has.L || has.P) && !hasG10) {
      return "L/P requires G10"
    }
    if (has.R) {
      return "R requires M110"
    }
    if (hasG10 && hasMotionG) {
      return "axis words conflict"
    }
    if (hasG33 && !hasZ && !hasX) {
      return "G33 requires axis move"
    }
    if (hasK && (this.motion !== Motion.Thread || hasG10)) {
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

    const axes = (hasZ ? AXIS_Z : 0) | (hasX ? AXIS_X : 0)

    if (hasG10) {
      if (val.L !== 20 || val.P !== 1) {
        return "only G10 L20 P1"
      }
      if (!axes) {
        return "G10 requires axis"
      }
      let newZDu = this.zDu
      let newXDu = this.xDu
      if (hasZ) {
        const z = scaledValueToDu(zVal, this.unitScaleDu)
        if (z === null) {
          return "value out of range"
        }
        newZDu = z
      }
      if (hasX) {
        const x = scaledValueToDu(xVal, this.unitScaleDu)
        if (x === null) {
          return "value out of range"
        }
        newXDu = x
      }
      this.zDu = newZDu
      this.xDu = newXDu
      sink.emit({ kind: "set-position", zDu: newZDu, xDu: newXDu, feed: 0, line: lineNo, axes })
    } else if (axes) {
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
