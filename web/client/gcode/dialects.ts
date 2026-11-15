// Dialect registry, mirroring main/lib/Gcode/GcodeDialects.{hpp,cpp}.

import type { Dialect } from "./core"
import type { Interpreter } from "./interpreter"
import { detectLathecode, Lathecode } from "./lathecode"

// Saved programs of this dialect are routines, called by name.
export const ROUTINE_DIALECT = "ngc"

// Standard words, no translation.
class Linuxcnc implements Dialect {
  constructor(private readonly dialectId: string) {}

  id(): string {
    return this.dialectId
  }

  begin(interp: Interpreter, startZDu: number, startXDu: number): void {
    interp.begin(startZDu, startXDu)
  }

  translate(): string | null {
    return null
  }
}

interface Entry {
  dialect: Dialect
  detect: (text: string) => boolean
}

const REGISTRY: Entry[] = [
  { dialect: new Lathecode(), detect: detectLathecode },
  { dialect: new Linuxcnc(ROUTINE_DIALECT), detect: (text) => /o\s*<[^>]+>\s*sub/i.test(stripComments(text)) },
  // Any text not detected otherwise.
  { dialect: new Linuxcnc("linuxcnc"), detect: () => true },
]

export function find(id: string): Dialect | null {
  return REGISTRY.find((e) => e.dialect.id() === id)?.dialect ?? null
}

export function list(): Dialect[] {
  return REGISTRY.map((e) => e.dialect)
}

// The id of the first dialect whose detector matches, else null.
export function detectDialect(text: string): string | null {
  return REGISTRY.find((e) => e.detect(text))?.dialect.id() ?? null
}

function stripComments(text: string): string {
  return text.replace(/\([^)\n]*\)?/g, "").replace(/;.*$/gm, "")
}

// Parameters, expressions, O-words or G38: only the firmware runs these.
export function usesNgc(text: string): boolean {
  const code = stripComments(text)
  return /[#[]/.test(code) || /^\s*(n\s*\d+\s*)?o/im.test(code) || /g\s*0*38/i.test(code)
}
