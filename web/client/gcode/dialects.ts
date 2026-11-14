// Dialect registry, mirroring main/lib/Gcode/GcodeDialects.{hpp,cpp}.

import type { Dialect } from "./core"
import type { Interpreter } from "./interpreter"
import { detectLathecode, Lathecode } from "./lathecode"

// Standard words, no translation.
class Linuxcnc implements Dialect {
  id(): string {
    return "linuxcnc"
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
  { dialect: new Linuxcnc(), detect: () => false },
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
