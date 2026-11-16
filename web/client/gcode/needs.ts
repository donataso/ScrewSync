import type { Capabilities } from "../types"
import { ROUTINE_DIALECT, stripComments } from "./dialects"

function code(text: string): string {
  return stripComments(text).replace(/<[^>]*>/g, "")
}

export function hasXWord(text: string): boolean {
  return /(?<![a-z0-9_])x\s*[-+.\d#[]/i.test(code(text))
}

export function usesProbe(text: string): boolean {
  return /(?<![a-z0-9_])g\s*0*38(?!\d)/i.test(code(text))
}

export function unavailableReason(
  text: string,
  dialectId: string,
  capabilities: Pick<Capabilities, "has_x_motor" | "has_probe"> | null,
): string | null {
  if (!capabilities) {
    return null
  }
  if (!capabilities.has_x_motor && dialectId !== ROUTINE_DIALECT && hasXWord(text)) {
    return "This program has an X word, but the X motor is off or not wired. Change this in Device and Pins."
  }
  if (!capabilities.has_probe && usesProbe(text)) {
    return "This program uses G38 probe moves, but the probe is off or not wired. Change this in Device and Pins."
  }
  return null
}
