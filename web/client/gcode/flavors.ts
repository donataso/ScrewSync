// Flavor registry, mirroring main/lib/Gcode/GcodeFlavors.{hpp,cpp}.

import type { Flavor } from "./core"
import { detectLathecode, Lathecode } from "./lathecode"

interface Entry {
  flavor: Flavor
  detect: (text: string) => boolean
}

const REGISTRY: Entry[] = [{ flavor: new Lathecode(), detect: detectLathecode }]

export function find(id: string): Flavor | null {
  return REGISTRY.find((e) => e.flavor.id() === id)?.flavor ?? null
}

export function list(): Flavor[] {
  return REGISTRY.map((e) => e.flavor)
}

// The id of the first flavor whose detector matches, else null.
export function detectFlavor(text: string): string | null {
  return REGISTRY.find((e) => e.detect(text))?.flavor.id() ?? null
}
