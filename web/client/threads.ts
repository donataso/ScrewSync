// Mirrors main/lib/Thread/ThreadTable.hpp and its validate().
export const MAX_GROUPS = 9
export const MAX_GROUP_ENTRIES = 35
export const NAME_MAX_LEN = 11
export const DESIGNATION_MAX_LEN = 15
export const MAX_DEPTH_FACTOR = 2
export const MAX_TAPER = 1
export const MAX_FLANK_ANGLE = 120

const DU_PER_MM = 10000
const DU_PER_INCH = 254000

export type Units = "mm" | "inch"

interface GroupJson {
  name: string
  depthExternal: number
  depthInternal: number
  taper: number
  flankAngle: number
}

interface EntryJson {
  group: string
  designation: string
  majorDu: number
  pitchDu: number
}

interface OverrideJson {
  group: string
  designation: string
  depthExternalDu: number
  depthInternalDu: number
  hasTaper: boolean
  taper: number
}

export interface ThreadTableJson {
  groups: GroupJson[]
  entries: EntryJson[]
  overrides?: OverrideJson[]
}

export interface ThreadEntry {
  designation: string
  majorDu: number
  pitchDu: number
  depthExternalDu?: number
  depthInternalDu?: number
  taper?: number
}

// Depth factors kept as typed text so invalid input stays visible.
export interface ThreadGroup {
  name: string
  depthExternal: string
  depthInternal: string
  taper: number
  flankAngle: string
  entries: ThreadEntry[]
}

export function fromJson(json: ThreadTableJson): ThreadGroup[] {
  const groups: ThreadGroup[] = json.groups.map((g) => ({
    name: g.name,
    depthExternal: String(g.depthExternal),
    depthInternal: String(g.depthInternal),
    taper: g.taper,
    flankAngle: String(g.flankAngle),
    entries: [],
  }))
  const byName = new Map(groups.map((g) => [g.name, g]))
  for (const e of json.entries) {
    byName.get(e.group)?.entries.push({ designation: e.designation, majorDu: e.majorDu, pitchDu: e.pitchDu })
  }
  for (const o of json.overrides ?? []) {
    const e = byName.get(o.group)?.entries.find((x) => x.designation === o.designation)
    if (!e) continue
    if (o.depthExternalDu) e.depthExternalDu = o.depthExternalDu
    if (o.depthInternalDu) e.depthInternalDu = o.depthInternalDu
    if (o.hasTaper) e.taper = o.taper
  }
  return groups
}

export function toJson(groups: ThreadGroup[]): ThreadTableJson {
  const json: ThreadTableJson = { groups: [], entries: [], overrides: [] }
  for (const g of groups) {
    json.groups.push({
      name: g.name,
      depthExternal: Number(g.depthExternal),
      depthInternal: Number(g.depthInternal),
      taper: g.taper,
      flankAngle: Number(g.flankAngle),
    })
    for (const e of g.entries) {
      json.entries.push({ group: g.name, designation: e.designation, majorDu: e.majorDu, pitchDu: e.pitchDu })
      if (e.depthExternalDu !== undefined || e.depthInternalDu !== undefined || e.taper !== undefined) {
        json.overrides!.push({
          group: g.name,
          designation: e.designation,
          depthExternalDu: e.depthExternalDu ?? 0,
          depthInternalDu: e.depthInternalDu ?? 0,
          hasTaper: e.taper !== undefined,
          taper: e.taper ?? 0,
        })
      }
    }
  }
  return json
}

export function hasOverride(e: ThreadEntry): boolean {
  return e.depthExternalDu !== undefined || e.depthInternalDu !== undefined || e.taper !== undefined
}

export function groupDepthDu(g: ThreadGroup, pitchDu: number, internal: boolean): number {
  return Math.round(Number(internal ? g.depthInternal : g.depthExternal) * Math.abs(pitchDu))
}

function trim(n: number, decimals: number): string {
  return String(Number(n.toFixed(decimals)))
}

export function lengthUnit(units: Units): string {
  return units === "inch" ? "in" : "mm"
}

export function formatLength(du: number, units: Units): string {
  return units === "inch" ? trim(du / DU_PER_INCH, 5) : trim(du / DU_PER_MM, 4)
}

export function parseLength(text: string, units: Units): number {
  if (text.trim() === "") return NaN
  return Math.round(Number(text) * (units === "inch" ? DU_PER_INCH : DU_PER_MM))
}

export function formatTpi(pitchDu: number): string {
  return trim(DU_PER_INCH / pitchDu, 3)
}

export function parseTpi(text: string): number {
  const tpi = Number(text)
  return text.trim() === "" || !(tpi > 0) ? NaN : Math.round(DU_PER_INCH / tpi)
}

export function nameError(name: string, taken: string[], what: string, maxLen: number): string | undefined {
  if (!name) return `${what} is empty`
  if (name.length > maxLen) return `At most ${maxLen} characters`
  if (/[ ,]/.test(name)) return "No spaces or commas"
  if (taken.includes(name)) return `${what} already exists`
  return undefined
}

export function factorError(text: string): string | undefined {
  const v = Number(text)
  if (text.trim() === "" || Number.isNaN(v)) return "Not a number"
  if (!(v > 0 && v < MAX_DEPTH_FACTOR)) return `Must be between 0 and ${MAX_DEPTH_FACTOR}`
  return undefined
}

export function flankAngleError(text: string): string | undefined {
  const v = Number(text)
  if (text.trim() === "" || Number.isNaN(v)) return "Not a number"
  if (!(v > 0 && v < MAX_FLANK_ANGLE)) return `Must be between 0 and ${MAX_FLANK_ANGLE}°`
  return undefined
}

export function taperError(ratio: number | undefined): string | undefined {
  if (ratio === undefined) return undefined
  if (Number.isNaN(ratio)) return "Not a number"
  if (!(Math.abs(ratio) < MAX_TAPER)) return `Must be under ${trim(ratioToDegrees(MAX_TAPER), 2)}° (1:${MAX_TAPER})`
  return undefined
}

export function groupErrors(groups: ThreadGroup[], index: number): Record<string, string | undefined> {
  const g = groups[index]
  const others = groups.filter((_, i) => i !== index).map((x) => x.name)
  return {
    name: nameError(g.name, others, "Group", NAME_MAX_LEN),
    depthExternal: factorError(g.depthExternal),
    depthInternal: factorError(g.depthInternal),
    flankAngle: flankAngleError(g.flankAngle),
    taper: taperError(g.taper),
  }
}

export function tableValid(groups: ThreadGroup[]): boolean {
  return (
    groups.length <= MAX_GROUPS &&
    groups.every((g, i) => g.entries.length <= MAX_GROUP_ENTRIES && Object.values(groupErrors(groups, i)).every((e) => !e))
  )
}

// Taper is stored as diameter change per length; the half-angle is atan(ratio / 2).
export function ratioToDegrees(ratio: number): number {
  return (Math.atan(ratio / 2) * 180) / Math.PI
}

export function degreesToRatio(degrees: number): number {
  return 2 * Math.tan((degrees * Math.PI) / 180)
}

export function formatDegrees(ratio: number): string {
  return trim(ratioToDegrees(Math.abs(ratio)), 4)
}

export function formatRatioN(ratio: number): string {
  return ratio === 0 ? "" : trim(1 / Math.abs(ratio), 3)
}

export function newGroupName(groups: ThreadGroup[]): string {
  const names = groups.map((g) => g.name)
  for (let i = 1; ; i++) {
    const name = i === 1 ? "new" : `new${i}`
    if (!names.includes(name)) return name
  }
}
