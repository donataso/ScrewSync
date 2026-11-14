export type PowerLabel = "5V" | "GND"
export type BoardPin = { gpio: number } | { power: PowerLabel }

export interface Connector {
  id: string
  label: string
  row: number
  col: number
  pins: BoardPin[]
}

export interface Board {
  name: string
  connectors: Connector[]
  reserved: Record<number, string>
  strapping: number[]
  pcntUnits: number
  /** Longest PCNT glitch filter. */
  maxHwFilterUs: number
  /** Longest sw debounce time. */
  maxSwFilterUs: number
  /** The firmware's function table. */
  functions: FunctionDef[]
}

export type Direction = "in" | "out"
export type Pull = "none" | "up" | "down"
export type EncoderType = "hw" | "sw"

export interface FunctionDef {
  id: string
  label: string
  group: string
  signals: string[]
  dir: Direction
  required: boolean
  /** At most one assignment. */
  exclusive: boolean
  /** Default invert of new assignments. */
  activeLow: boolean
  encoderTypes?: EncoderType[]
  /** Default encoder filter of new assignments. */
  filterUs: number
}

export interface SignalOptions {
  invert: boolean
  pull: Pull
}

export interface Assignment {
  fn: string
  connector: string
  start: number
  encoderType?: EncoderType
  /** PCNT glitch filter (hw) or debounce time (sw). */
  filterUs: number
  options: SignalOptions[]
}

/** In saved order. A non-exclusive function can be in it more than once. */
export type PinMap = Assignment[]

/** User names of connectors, by connector id. A missing name means the board's label. */
export type ConnectorNames = Record<string, string>

export const CONNECTOR_NAME_MAX = 20

export interface Issue {
  level: "error" | "warn"
  msg: string
  fn?: string
  /** Assignment index. */
  index?: number
  gpio?: number
  connector?: string
}

export interface PinUse {
  index: number
  fn: string
  signal: string
}

const MOTOR = ["ENA", "DIR", "STEP"]
const QUAD = ["A", "B"]

// The firmware's function table, for tests.
export const MOCK_FUNCTIONS: FunctionDef[] = [
  { id: "zMotor", label: "Z motor", group: "Motors", signals: MOTOR, dir: "out", required: true, exclusive: true, activeLow: false, filterUs: 0 },
  { id: "xMotor", label: "X motor", group: "Motors", signals: MOTOR, dir: "out", required: false, exclusive: true, activeLow: false, filterUs: 0 },
  { id: "spindle", label: "Spindle encoder", group: "Encoders", signals: QUAD, dir: "in", required: true, exclusive: true, activeLow: false, encoderTypes: ["hw"], filterUs: 0.013 },
  { id: "zHandwheel", label: "Z handwheel", group: "Encoders", signals: QUAD, dir: "in", required: false, exclusive: true, activeLow: false, encoderTypes: ["hw", "sw"], filterUs: 12.787 },
  { id: "xHandwheel", label: "X handwheel", group: "Encoders", signals: QUAD, dir: "in", required: false, exclusive: true, activeLow: false, encoderTypes: ["hw", "sw"], filterUs: 12.787 },
  { id: "probe", label: "Probe", group: "Inputs", signals: ["SIG"], dir: "in", required: false, exclusive: false, activeLow: false, filterUs: 0 },
  { id: "stop", label: "Stop", group: "Inputs", signals: ["SIG"], dir: "in", required: false, exclusive: false, activeLow: true, filterUs: 0 },
]

const g = (gpio: number): BoardPin => ({ gpio })
const V5: BoardPin = { power: "5V" }
const GND: BoardPin = { power: "GND" }

function connector(row: number, col: number, label: string, pins: BoardPin[]): Connector {
  return { id: `r${row}c${col}`, label, row, col, pins }
}

const PSRAM_NOTE = "octal PSRAM data line inside the chip package. Driving it corrupts the PSRAM and breaks Wi-Fi"

export const STRAPPING_NOTE = "its level at reset selects the boot mode. A pull or a load on it can stop the board from booting"

// The nanoels H5 board, for tests.
export const MOCK_BOARD: Board = {
  name: "nanoels H5",
  connectors: [
    connector(1, 1, "Spare 1", [V5, g(41), g(42), g(35)]),
    connector(1, 2, "Z motor", [V5, g(16), g(15), g(7)]),
    connector(1, 3, "X motor", [V5, g(1), g(2), g(17)]),
    connector(1, 4, "Spare 2", [g(38), g(6), g(5), g(4)]),
    connector(1, 5, "Spare 3", [V5, GND, g(36), g(37)]),
    connector(2, 1, "PROBE", [V5, GND, g(8), g(18)]),
    connector(2, 2, "Z handwheel", [V5, GND, g(21), g(47)]),
    connector(2, 3, "X handwheel", [V5, GND, g(3), g(46)]),
    connector(2, 4, "POWER", [V5, V5, GND, GND]),
    connector(2, 5, "Spare 4", [V5, GND, g(14), g(13)]),
    connector(3, 1, "Spare 5", [V5, GND, g(12), g(9)]),
    connector(3, 2, "Spare 6", [V5, GND, g(39), g(40)]),
    connector(3, 3, "Spare 7", [V5, GND, g(48), g(45)]),
    connector(3, 4, "POWER", [V5, V5, GND, GND]),
    connector(3, 5, "ENCODER", [V5, GND, g(10), g(11)]),
  ],
  reserved: {
    35: PSRAM_NOTE,
    36: PSRAM_NOTE,
    37: PSRAM_NOTE,
  },
  strapping: [0, 3, 45, 46],
  pcntUnits: 4,
  maxHwFilterUs: 12.787,
  maxSwFilterUs: 5000,
  functions: MOCK_FUNCTIONS,
}

const OUT: SignalOptions = { invert: false, pull: "none" }
const UP: SignalOptions = { invert: false, pull: "up" }

export const MOCK_MAP: PinMap = [
  { fn: "zMotor", connector: "r1c2", start: 1, filterUs: 0, options: [OUT, OUT, OUT] },
  { fn: "xMotor", connector: "r1c3", start: 1, filterUs: 0, options: [OUT, OUT, OUT] },
  { fn: "spindle", connector: "r3c5", start: 2, encoderType: "hw", filterUs: 0.013, options: [UP, UP] },
  { fn: "zHandwheel", connector: "r2c2", start: 2, encoderType: "hw", filterUs: 12.787, options: [UP, UP] },
  { fn: "xHandwheel", connector: "r2c3", start: 2, encoderType: "hw", filterUs: 12.787, options: [UP, UP] },
  { fn: "probe", connector: "r2c1", start: 2, filterUs: 0, options: [UP] },
]

export function connectorLabel(board: Board, names: ConnectorNames, id: string): string {
  return names[id] ?? connectorOf(board, id)?.label ?? id
}

export function nameIssues(board: Board, names: ConnectorNames): Issue[] {
  const issues: Issue[] = []
  for (const [id, name] of Object.entries(names)) {
    if (!connectorOf(board, id)) {
      issues.push({ level: "error", connector: id, msg: `Name for unknown connector ${id}` })
    } else if (name.trim() === "" || name !== name.trim()) {
      issues.push({ level: "error", connector: id, msg: `Connector name "${name}" must not be empty or start or end with spaces` })
    } else if (name.length > CONNECTOR_NAME_MAX) {
      issues.push({ level: "error", connector: id, msg: `Connector name "${name}" is longer than ${CONNECTOR_NAME_MAX} characters` })
    } else if ([...name].some((ch) => ch.charCodeAt(0) < 32 || ch.charCodeAt(0) === 127)) {
      issues.push({ level: "error", connector: id, msg: `Connector name "${name}" has control characters` })
    }
  }
  return issues
}

export function functionDef(board: Board, id: string): FunctionDef | undefined {
  return board.functions.find((f) => f.id === id)
}

/** The function's label, with a number when it has more than one assignment. */
export function assignmentLabel(board: Board, map: PinMap, index: number): string {
  const a = map[index]
  const label = functionDef(board, a.fn)?.label ?? a.fn
  const same = map.filter((x) => x.fn === a.fn)
  return same.length > 1 ? `${label} ${same.indexOf(a) + 1}` : label
}

/** True if one more assignment of the function is allowed. */
export function canAdd(board: Board, map: PinMap, fnId: string): boolean {
  const fn = functionDef(board, fnId)
  return !!fn && (!fn.exclusive || !map.some((a) => a.fn === fnId))
}

export function connectorOf(board: Board, id: string): Connector | undefined {
  return board.connectors.find((c) => c.id === id)
}

/** GPIOs of an assignment in signal order; null where the slot is not a GPIO. */
export function pinsOf(board: Board, fn: FunctionDef, a: Assignment): (number | null)[] {
  const c = connectorOf(board, a.connector)
  return Array.from({ length: fn.signals.length }, (_, i) => {
    const p = c?.pins[a.start + i]
    return p && "gpio" in p ? p.gpio : null
  })
}

export function usage(board: Board, map: PinMap): Map<number, PinUse[]> {
  const used = new Map<number, PinUse[]>()
  map.forEach((a, index) => {
    const fn = functionDef(board, a.fn)
    if (!fn) {
      return
    }
    pinsOf(board, fn, a).forEach((gpio, i) => {
      if (gpio !== null) {
        used.set(gpio, [...(used.get(gpio) ?? []), { index, fn: fn.id, signal: fn.signals[i] }])
      }
    })
  })
  return used
}

/** True if fnId fits at start; the assignment at moving (if given) does not count as using its pins. */
export function canStartAt(board: Board, map: PinMap, fnId: string, connectorId: string, start: number, moving?: number): boolean {
  const fn = functionDef(board, fnId)
  const c = connectorOf(board, connectorId)
  if (!fn || !c || start < 0 || start + fn.signals.length > c.pins.length) {
    return false
  }
  const used = usage(
    board,
    map.filter((_, i) => i !== moving),
  )
  return c.pins.slice(start, start + fn.signals.length).every((p) => "gpio" in p && !(p.gpio in board.reserved) && !used.has(p.gpio))
}

export function placements(board: Board, map: PinMap, fnId: string, moving?: number): { connector: string; start: number }[] {
  return board.connectors.flatMap((c) =>
    c.pins.map((_, start) => ({ connector: c.id, start })).filter((p) => canStartAt(board, map, fnId, p.connector, p.start, moving)),
  )
}

/** Functions that can be added at a pin. */
export function functionsAt(board: Board, map: PinMap, connectorId: string, start: number): FunctionDef[] {
  return board.functions.filter((fn) => canAdd(board, map, fn.id) && canStartAt(board, map, fn.id, connectorId, start))
}

export function defaultAssignment(fn: FunctionDef, connectorId: string, start: number): Assignment {
  const opt = { ...(fn.dir === "in" ? UP : OUT), invert: fn.activeLow }
  return {
    fn: fn.id,
    connector: connectorId,
    start,
    encoderType: fn.encoderTypes?.[0],
    filterUs: fn.filterUs,
    options: fn.signals.map(() => ({ ...opt })),
  }
}

export function maxFilterUs(board: Board, a: Assignment): number {
  return a.encoderType === "sw" ? board.maxSwFilterUs : board.maxHwFilterUs
}

export function validate(board: Board, map: PinMap): Issue[] {
  const issues: Issue[] = []
  for (const fn of board.functions) {
    if (fn.required && !map.some((a) => a.fn === fn.id)) {
      issues.push({ level: "error", fn: fn.id, msg: `${fn.label} has no pins` })
    }
  }
  map.forEach((a, index) => {
    const fn = functionDef(board, a.fn)
    if (!fn) {
      issues.push({ level: "error", index, msg: `Unknown function ${a.fn}` })
      return
    }
    const label = assignmentLabel(board, map, index)
    if (fn.exclusive && map.findIndex((x) => x.fn === a.fn) !== index) {
      issues.push({ level: "error", fn: fn.id, index, msg: `${fn.label} is given more than once` })
    }
    if (!connectorOf(board, a.connector)) {
      issues.push({ level: "error", fn: fn.id, index, msg: `${label}: unknown connector` })
      return
    }
    const pins = pinsOf(board, fn, a)
    if (pins.some((p) => p === null)) {
      issues.push({ level: "error", fn: fn.id, index, msg: `${label}: not all signals are on GPIO pins` })
    }
    for (const gpio of pins) {
      if (gpio === null) {
        continue
      }
      if (gpio in board.reserved) {
        issues.push({ level: "error", fn: fn.id, index, gpio, msg: `${label}: GPIO ${gpio} is reserved (${board.reserved[gpio]})` })
      }
      if (board.strapping.includes(gpio)) {
        issues.push({ level: "warn", fn: fn.id, index, gpio, msg: `${label}: GPIO ${gpio} is a strapping pin` })
      }
    }
    if (a.options.length !== fn.signals.length) {
      issues.push({ level: "error", fn: fn.id, index, msg: `${label}: options do not match its signals` })
    }
    if (fn.encoderTypes && (!a.encoderType || !fn.encoderTypes.includes(a.encoderType))) {
      issues.push({ level: "error", fn: fn.id, index, msg: `${label}: encoder type must be ${fn.encoderTypes.join(" or ")}` })
    }
    const maxFilter = maxFilterUs(board, a)
    if (!Number.isFinite(a.filterUs) || a.filterUs < 0 || a.filterUs > maxFilter) {
      issues.push({ level: "error", fn: fn.id, index, msg: `${label}: filter must be 0 to ${maxFilter} µs` })
    }
  })
  for (const [gpio, uses] of usage(board, map)) {
    if (uses.length > 1) {
      const names = uses.map((u) => `${assignmentLabel(board, map, u.index)} ${u.signal}`).join(", ")
      issues.push({ level: "error", gpio, msg: `GPIO ${gpio} is used more than once: ${names}` })
    }
  }
  const hw = map.filter((a) => a.encoderType === "hw").length
  if (hw > board.pcntUnits) {
    issues.push({ level: "error", msg: `${hw} hardware encoders, but the board has ${board.pcntUnits} PCNT units` })
  }
  return issues
}

interface PinsJsonSignal {
  invert: boolean
  pull: Pull
}

interface PinsJsonFunction {
  id: string
  connector: string
  start: number
  encoder: "" | EncoderType
  filterUs: number
  signals: PinsJsonSignal[]
}

interface PinsJsonName {
  connector: string
  name: string
}

export interface PinsUpdateJson {
  functions: PinsJsonFunction[]
  names: PinsJsonName[]
}

/** GET /pins/functions. */
export interface PinFunctionsJson {
  functions: {
    id: string
    label: string
    group: string
    signals: string[]
    dir: Direction
    required: boolean
    exclusive: boolean
    activeLow: boolean
    encoders: EncoderType[]
    filterUs: number
  }[]
}

/** GET /pins. Pins: GPIO number, -1 = 5V, -2 = GND. */
export interface PinsJson extends PinsUpdateJson {
  board: {
    name: string
    pcntUnits: number
    maxHwFilterUs: number
    maxSwFilterUs: number
    strapping: number[]
    reserved: { gpio: number; note: string }[]
    connectors: { id: string; title: string; row: number; col: number; pins: number[] }[]
  }
}

export function fromPinsJson(doc: PinsJson, fns: PinFunctionsJson): { board: Board; map: PinMap; names: ConnectorNames } {
  const board: Board = {
    functions: fns.functions.map((f) => ({
      id: f.id,
      label: f.label,
      group: f.group,
      signals: f.signals,
      dir: f.dir,
      required: f.required,
      exclusive: f.exclusive,
      activeLow: f.activeLow,
      encoderTypes: f.encoders.length ? f.encoders : undefined,
      filterUs: f.filterUs,
    })),
    name: doc.board.name,
    pcntUnits: doc.board.pcntUnits,
    maxHwFilterUs: doc.board.maxHwFilterUs,
    maxSwFilterUs: doc.board.maxSwFilterUs,
    strapping: doc.board.strapping,
    reserved: Object.fromEntries(doc.board.reserved.map((r) => [r.gpio, r.note])),
    connectors: doc.board.connectors.map((c) => ({
      id: c.id,
      label: c.title,
      row: c.row,
      col: c.col,
      pins: c.pins.map((p): BoardPin => (p === -1 ? { power: "5V" } : p === -2 ? { power: "GND" } : { gpio: p })),
    })),
  }
  const map: PinMap = (doc.functions ?? []).map((f) => ({
    fn: f.id,
    connector: f.connector,
    start: f.start,
    encoderType: f.encoder || undefined,
    filterUs: f.filterUs,
    options: f.signals.map((s) => ({ invert: s.invert, pull: s.pull })),
  }))
  const names: ConnectorNames = Object.fromEntries((doc.names ?? []).map((n) => [n.connector, n.name]))
  return { board, map, names }
}

export function toPinsJson(map: PinMap, names: ConnectorNames): PinsUpdateJson {
  return {
    functions: map.map((a) => ({
      id: a.fn,
      connector: a.connector,
      start: a.start,
      encoder: a.encoderType ?? "",
      filterUs: a.filterUs,
      signals: a.options.map((o) => ({ invert: o.invert, pull: o.pull })),
    })),
    names: Object.entries(names).map(([connector, name]) => ({ connector, name })),
  }
}
