// Dialect for https://kachurovskiy.com/lathecode/ output: the standard set
// with G32 instead of G33, its F as the modal thread lead. Starts in G1.
// Mirrors main/lib/Gcode/GcodeLathecode.{hpp,cpp}.

import { DU_PER_INCH, DU_PER_MM, MAX_WORDS, type Dialect, type Step, type Word } from "./core"
import { type Interpreter, Motion } from "./interpreter"

export class Lathecode implements Dialect {
  private thread = false
  private leadSet = false
  private leadDu = 0
  private unitScaleDu: number = DU_PER_MM

  id(): string {
    return "lathecode"
  }

  begin(interp: Interpreter, startZDu: number, startXDu: number): void {
    interp.begin(startZDu, startXDu, Motion.Feed)
    this.thread = false
    this.leadSet = false
    this.leadDu = 0
    this.unitScaleDu = DU_PER_MM
  }

  translate(words: Word[]): string | null {
    let hasAxis = false
    let f: Word | null = null
    for (const w of words) {
      switch (w.letter) {
        case "G":
          if (w.value === 32) {
            this.thread = true
            w.value = 33
          } else if (w.value === 33) {
            return "unsupported command"
          } else if (w.value === 0 || w.value === 1) {
            this.thread = false
          } else if (w.value === 20) {
            this.unitScaleDu = DU_PER_INCH
          } else if (w.value === 21) {
            this.unitScaleDu = DU_PER_MM
          }
          break
        case "K":
          return "unsupported axis"
        case "F":
          if (f) {
            return "duplicate word"
          }
          f = w
          break
        case "Z":
        case "X":
          hasAxis = true
          break
      }
    }
    if (!this.thread) {
      return null
    }
    if (f) {
      f.letter = "K"
      this.leadDu = f.value * this.unitScaleDu
      this.leadSet = true
    } else if (hasAxis && this.leadSet) {
      if (words.length >= MAX_WORDS) {
        return "too many words"
      }
      words.push({ letter: "K", value: this.leadDu / this.unitScaleDu })
    }
    return null
  }
}

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
