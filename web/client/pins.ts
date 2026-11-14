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
}

export type Direction = "in" | "out"
export type Pull = "none" | "up" | "down"
export type EncoderType = "hw" | "sw"

export interface FunctionDef {
  id: string
  label: string
  signals: string[]
  dir: Direction
  required: boolean
  encoderTypes?: EncoderType[]
}

export interface SignalOptions {
  invert: boolean
  pull: Pull
}

export interface Assignment {
  connector: string
  start: number
  encoderType?: EncoderType
  options: SignalOptions[]
}

export type PinMap = Record<string, Assignment>

/** User names of connectors, by connector id. A missing name means the board's label. */
export type ConnectorNames = Record<string, string>

export const CONNECTOR_NAME_MAX = 20

export interface Issue {
  level: "error" | "warn"
  msg: string
  fn?: string
  gpio?: number
  connector?: string
}

export interface PinUse {
  fn: string
  signal: string
}

export const FUNCTIONS: FunctionDef[] = [
  { id: "zMotor", label: "Z motor", signals: ["ENA", "DIR", "STEP"], dir: "out", required: true },
  { id: "xMotor", label: "X motor", signals: ["ENA", "DIR", "STEP"], dir: "out", required: false },
  { id: "spindle", label: "Spindle encoder", signals: ["A", "B"], dir: "in", required: true, encoderTypes: ["hw"] },
  { id: "zHandwheel", label: "Z handwheel", signals: ["A", "B"], dir: "in", required: false, encoderTypes: ["hw", "sw"] },
  { id: "xHandwheel", label: "X handwheel", signals: ["A", "B"], dir: "in", required: false, encoderTypes: ["hw", "sw"] },
  { id: "probe", label: "Probe", signals: ["SIG"], dir: "in", required: false },
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
}

const OUT: SignalOptions = { invert: false, pull: "none" }
const UP: SignalOptions = { invert: false, pull: "up" }

export const MOCK_MAP: PinMap = {
  zMotor: { connector: "r1c2", start: 1, options: [OUT, OUT, OUT] },
  xMotor: { connector: "r1c3", start: 1, options: [OUT, OUT, OUT] },
  spindle: { connector: "r3c5", start: 2, encoderType: "hw", options: [UP, UP] },
  zHandwheel: { connector: "r2c2", start: 2, encoderType: "hw", options: [UP, UP] },
  xHandwheel: { connector: "r2c3", start: 2, encoderType: "hw", options: [UP, UP] },
  probe: { connector: "r2c1", start: 2, options: [UP] },
}

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

export function functionDef(id: string): FunctionDef | undefined {
  return FUNCTIONS.find((f) => f.id === id)
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
  for (const fn of FUNCTIONS) {
    const a = map[fn.id]
    if (!a) {
      continue
    }
    pinsOf(board, fn, a).forEach((gpio, i) => {
      if (gpio !== null) {
        used.set(gpio, [...(used.get(gpio) ?? []), { fn: fn.id, signal: fn.signals[i] }])
      }
    })
  }
  return used
}

export function canStartAt(board: Board, map: PinMap, fnId: string, connectorId: string, start: number): boolean {
  const fn = functionDef(fnId)
  const c = connectorOf(board, connectorId)
  if (!fn || !c || start < 0 || start + fn.signals.length > c.pins.length) {
    return false
  }
  const others = { ...map }
  delete others[fnId]
  const used = usage(board, others)
  return c.pins.slice(start, start + fn.signals.length).every((p) => "gpio" in p && !(p.gpio in board.reserved) && !used.has(p.gpio))
}

export function placements(board: Board, map: PinMap, fnId: string): { connector: string; start: number }[] {
  return board.connectors.flatMap((c) =>
    c.pins.map((_, start) => ({ connector: c.id, start })).filter((p) => canStartAt(board, map, fnId, p.connector, p.start)),
  )
}

export function functionsAt(board: Board, map: PinMap, connectorId: string, start: number): FunctionDef[] {
  return FUNCTIONS.filter((fn) => canStartAt(board, map, fn.id, connectorId, start))
}

export function defaultAssignment(fn: FunctionDef, connectorId: string, start: number): Assignment {
  const opt = fn.dir === "in" ? UP : OUT
  return {
    connector: connectorId,
    start,
    encoderType: fn.encoderTypes?.[0],
    options: fn.signals.map(() => ({ ...opt })),
  }
}

export function validate(board: Board, map: PinMap): Issue[] {
  const issues: Issue[] = []
  for (const fn of FUNCTIONS) {
    const a = map[fn.id]
    if (!a) {
      if (fn.required) {
        issues.push({ level: "error", fn: fn.id, msg: `${fn.label} has no pins` })
      }
      continue
    }
    if (!connectorOf(board, a.connector)) {
      issues.push({ level: "error", fn: fn.id, msg: `${fn.label}: unknown connector` })
      continue
    }
    const pins = pinsOf(board, fn, a)
    if (pins.some((p) => p === null)) {
      issues.push({ level: "error", fn: fn.id, msg: `${fn.label}: not all signals are on GPIO pins` })
    }
    for (const gpio of pins) {
      if (gpio === null) {
        continue
      }
      if (gpio in board.reserved) {
        issues.push({ level: "error", fn: fn.id, gpio, msg: `${fn.label}: GPIO ${gpio} is reserved (${board.reserved[gpio]})` })
      }
      if (board.strapping.includes(gpio)) {
        issues.push({ level: "warn", fn: fn.id, gpio, msg: `${fn.label}: GPIO ${gpio} is a strapping pin` })
      }
    }
    if (a.options.length !== fn.signals.length) {
      issues.push({ level: "error", fn: fn.id, msg: `${fn.label}: options do not match its signals` })
    }
    if (fn.encoderTypes && (!a.encoderType || !fn.encoderTypes.includes(a.encoderType))) {
      issues.push({ level: "error", fn: fn.id, msg: `${fn.label}: encoder type must be ${fn.encoderTypes.join(" or ")}` })
    }
  }
  for (const [gpio, uses] of usage(board, map)) {
    if (uses.length > 1) {
      const names = uses.map((u) => `${functionDef(u.fn)?.label} ${u.signal}`).join(", ")
      issues.push({ level: "error", gpio, msg: `GPIO ${gpio} is used more than once: ${names}` })
    }
  }
  const hw = FUNCTIONS.filter((fn) => map[fn.id]?.encoderType === "hw").length
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

/** GET /pins. Pins: GPIO number, -1 = 5V, -2 = GND. */
export interface PinsJson extends PinsUpdateJson {
  board: {
    name: string
    pcntUnits: number
    strapping: number[]
    reserved: { gpio: number; note: string }[]
    connectors: { id: string; title: string; row: number; col: number; pins: number[] }[]
  }
}

export function fromPinsJson(doc: PinsJson): { board: Board; map: PinMap; names: ConnectorNames } {
  const board: Board = {
    name: doc.board.name,
    pcntUnits: doc.board.pcntUnits,
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
  const map: PinMap = {}
  for (const f of doc.functions ?? []) {
    map[f.id] = {
      connector: f.connector,
      start: f.start,
      encoderType: f.encoder || undefined,
      options: f.signals.map((s) => ({ invert: s.invert, pull: s.pull })),
    }
  }
  const names: ConnectorNames = Object.fromEntries((doc.names ?? []).map((n) => [n.connector, n.name]))
  return { board, map, names }
}

export function toPinsJson(map: PinMap, names: ConnectorNames): PinsUpdateJson {
  return {
    functions: FUNCTIONS.filter((fn) => map[fn.id]).map((fn) => {
      const a = map[fn.id]
      return {
        id: fn.id,
        connector: a.connector,
        start: a.start,
        encoder: a.encoderType ?? "",
        signals: a.options.map((o) => ({ invert: o.invert, pull: o.pull })),
      }
    }),
    names: Object.entries(names).map(([connector, name]) => ({ connector, name })),
  }
}
