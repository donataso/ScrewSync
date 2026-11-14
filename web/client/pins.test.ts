import { describe, expect, it } from "vitest"
import {
  CONNECTOR_NAME_MAX,
  MOCK_BOARD,
  MOCK_FUNCTIONS,
  MOCK_MAP,
  assignmentLabel,
  canAdd,
  canStartAt,
  connectorLabel,
  defaultAssignment,
  functionDef,
  fromPinsJson,
  functionsAt,
  nameIssues,
  pinsOf,
  placements,
  toPinsJson,
  validate,
  type PinMap,
  type PinFunctionsJson,
  type PinsJson,
} from "./pins"

const fn = (id: string) => functionDef(MOCK_BOARD, id)!
const at = (map: PinMap, id: string) => map.findIndex((a) => a.fn === id)
const without = (id: string): PinMap => MOCK_MAP.filter((a) => a.fn !== id)
const replace = (id: string, change: Partial<PinMap[number]>): PinMap => MOCK_MAP.map((a) => (a.fn === id ? { ...a, ...change } : a))

describe("pinsOf", () => {
  it("gives the GPIOs in signal order", () => {
    expect(pinsOf(MOCK_BOARD, fn("zMotor"), MOCK_MAP[at(MOCK_MAP, "zMotor")])).toEqual([16, 15, 7])
  })

  it("gives null for power pins", () => {
    expect(pinsOf(MOCK_BOARD, fn("probe"), { ...MOCK_MAP[at(MOCK_MAP, "probe")], start: 0 })).toEqual([null])
  })
})

describe("validate", () => {
  it("accepts the default map, with strapping warnings only", () => {
    const issues = validate(MOCK_BOARD, MOCK_MAP)
    expect(issues.filter((i) => i.level === "error")).toEqual([])
    expect(issues.filter((i) => i.level === "warn").map((i) => i.gpio)).toEqual([3, 46])
  })

  it("checks the encoder edge rate against the filter", () => {
    const errors = (map: PinMap, ppr = MOCK_BOARD.ppr) =>
      validate({ ...MOCK_BOARD, ppr }, map).filter((i) => i.level === "error" && i.fn === "zHandwheel")
    expect(errors(replace("zHandwheel", { encoderType: "sw" }))).toHaveLength(1)
    expect(errors(replace("zHandwheel", { encoderType: "sw", filterUs: 0 }))).toHaveLength(1)
    expect(errors(replace("zHandwheel", { encoderType: "sw" }), { zHandwheel: 100 })).toEqual([])
    expect(errors(replace("zHandwheel", { encoderType: "sw", filterUs: 5000 }), {})).toEqual([])
  })

  it("rejects a missing required function", () => {
    const map = without("zMotor")
    expect(validate(MOCK_BOARD, map).some((i) => i.level === "error" && i.fn === "zMotor")).toBe(true)
  })

  it("accepts a missing optional function", () => {
    const map = without("xMotor")
    expect(validate(MOCK_BOARD, map).filter((i) => i.level === "error")).toEqual([])
  })

  it("rejects a GPIO used twice", () => {
    const map = replace("probe", { connector: "r3c5", start: 2 })
    expect(validate(MOCK_BOARD, map).some((i) => i.level === "error" && i.gpio === 10)).toBe(true)
  })

  it("rejects a reserved pin", () => {
    const map = replace("probe", { connector: "r1c1", start: 3 })
    expect(validate(MOCK_BOARD, map).some((i) => i.level === "error" && i.gpio === 35)).toBe(true)
  })

  it("rejects a group that runs onto a power pin", () => {
    const map = replace("xMotor", { connector: "r2c5", start: 1 })
    expect(validate(MOCK_BOARD, map).some((i) => i.level === "error" && i.fn === "xMotor")).toBe(true)
  })

  it("rejects more hardware encoders than PCNT units", () => {
    const board = { ...MOCK_BOARD, pcntUnits: 2 }
    expect(validate(board, MOCK_MAP).some((i) => i.level === "error" && i.msg.includes("PCNT"))).toBe(true)
  })

  it("rejects an encoder type the function does not allow", () => {
    const map = replace("spindle", { encoderType: "sw" })
    expect(validate(MOCK_BOARD, map).some((i) => i.level === "error" && i.fn === "spindle")).toBe(true)
  })

  it("limits the filter by encoder type", () => {
    const long = MOCK_BOARD.maxHwFilterUs + 1
    expect(validate(MOCK_BOARD, replace("zHandwheel", { filterUs: long })).some((i) => i.level === "error" && i.fn === "zHandwheel")).toBe(true)
    expect(validate({ ...MOCK_BOARD, ppr: {} }, replace("zHandwheel", { encoderType: "sw", filterUs: long })).filter((i) => i.level === "error")).toEqual([])
    expect(
      validate(MOCK_BOARD, replace("zHandwheel", { encoderType: "sw", filterUs: MOCK_BOARD.maxSwFilterUs + 1 })).some(
        (i) => i.level === "error" && i.fn === "zHandwheel",
      ),
    ).toBe(true)
  })

  it("rejects an exclusive function given twice", () => {
    const map: PinMap = [...MOCK_MAP, defaultAssignment(fn("xMotor"), "r1c4", 0)]
    expect(validate(MOCK_BOARD, map).some((i) => i.level === "error" && i.index === map.length - 1)).toBe(true)
  })

  it("accepts a non-exclusive function more than once", () => {
    const map: PinMap = [...MOCK_MAP, defaultAssignment(fn("probe"), "r2c1", 3)]
    expect(validate(MOCK_BOARD, map).filter((i) => i.level === "error")).toEqual([])
    expect(assignmentLabel(MOCK_BOARD, map, map.length - 1)).toBe("Probe 2")
    expect(assignmentLabel(MOCK_BOARD, MOCK_MAP, at(MOCK_MAP, "probe"))).toBe("Probe")
  })
})

describe("placement", () => {
  it("allows a group only on free GPIOs next to each other", () => {
    expect(canStartAt(MOCK_BOARD, MOCK_MAP, "xMotor", "r1c4", 0)).toBe(true)
    expect(canStartAt(MOCK_BOARD, MOCK_MAP, "xMotor", "r1c4", 2)).toBe(false)
    expect(canStartAt(MOCK_BOARD, MOCK_MAP, "xMotor", "r1c2", 1)).toBe(false)
    expect(canStartAt(MOCK_BOARD, MOCK_MAP, "xMotor", "r1c1", 1)).toBe(false)
  })

  it("allows a moved function on its own pins", () => {
    expect(canStartAt(MOCK_BOARD, MOCK_MAP, "zMotor", "r1c2", 1, at(MOCK_MAP, "zMotor"))).toBe(true)
    expect(canStartAt(MOCK_BOARD, MOCK_MAP, "zMotor", "r1c2", 1)).toBe(false)
  })

  it("adds only non-exclusive or unused functions", () => {
    expect(canAdd(MOCK_BOARD, MOCK_MAP, "probe")).toBe(true)
    expect(canAdd(MOCK_BOARD, MOCK_MAP, "xMotor")).toBe(false)
    expect(canAdd(MOCK_BOARD, without("xMotor"), "xMotor")).toBe(true)
  })

  it("lists every place a group fits", () => {
    const map = without("xMotor")
    expect(placements(MOCK_BOARD, map, "xMotor")).toEqual([
      { connector: "r1c3", start: 1 },
      { connector: "r1c4", start: 0 },
      { connector: "r1c4", start: 1 },
    ])
  })

  it("lists the functions that can be added at a pin", () => {
    expect(functionsAt(MOCK_BOARD, MOCK_MAP, "r3c2", 2).map((f) => f.id)).toEqual(["probe", "stop"])
    expect(functionsAt(MOCK_BOARD, without("zHandwheel"), "r3c2", 2).map((f) => f.id)).toEqual(["zHandwheel", "probe", "stop"])
  })

  it("gives inputs a pull-up by default", () => {
    expect(defaultAssignment(fn("probe"), "r3c2", 2).options).toEqual([{ invert: false, pull: "up" }])
    expect(defaultAssignment(fn("stop"), "r3c2", 2).options).toEqual([{ invert: true, pull: "up" }])
    expect(MOCK_FUNCTIONS.every((f) => !f.encoderTypes || defaultAssignment(f, "r3c2", 2).encoderType === f.encoderTypes[0])).toBe(true)
  })
})

describe("connector names", () => {
  it("uses the board label when there is no name", () => {
    expect(connectorLabel(MOCK_BOARD, {}, "r1c1")).toBe("Spare 1")
    expect(connectorLabel(MOCK_BOARD, { r1c1: "Coolant" }, "r1c1")).toBe("Coolant")
  })

  it("accepts good names, also the same name twice", () => {
    expect(nameIssues(MOCK_BOARD, { r1c1: "Coolant", r1c4: "Coolant" })).toEqual([])
  })

  it("rejects bad names", () => {
    const bad = {
      r1c1: "",
      r1c4: " Pad",
      r2c5: "x".repeat(CONNECTOR_NAME_MAX + 1),
      r3c1: "a\tb",
      nope: "Foo",
    }
    expect(nameIssues(MOCK_BOARD, bad).map((i) => i.connector)).toEqual(["r1c1", "r1c4", "r2c5", "r3c1", "nope"])
  })
})

describe("pins JSON", () => {
  const board: PinsJson["board"] = {
    name: "nanoels H5",
    pcntUnits: 4,
    maxHwFilterUs: 12.787,
    maxSwFilterUs: 5000,
    strapping: [3],
    reserved: [{ gpio: 35, note: "PSRAM" }],
    connectors: [
      { id: "r1c1", title: "Spare 1", row: 1, col: 1, pins: [-1, 41, 42, 35] },
      { id: "r2c1", title: "PROBE", row: 2, col: 1, pins: [-1, -2, 8, 18] },
    ],
  }
  const fns: PinFunctionsJson = {
    functions: MOCK_FUNCTIONS.map((f) => ({ ...f, encoders: f.encoderTypes ?? [] })),
  }

  it("reads the board", () => {
    const { board: b } = fromPinsJson({ board, functions: [], names: [] }, fns)
    expect(b.functions).toEqual(MOCK_FUNCTIONS)
    expect(b.connectors[0].pins).toEqual([{ power: "5V" }, { gpio: 41 }, { gpio: 42 }, { gpio: 35 }])
    expect(b.connectors[1].pins[1]).toEqual({ power: "GND" })
    expect(b.reserved).toEqual({ 35: "PSRAM" })
    expect(b.connectors[0].label).toBe("Spare 1")
  })

  it("gives back the same map and names", () => {
    const names = { r1c1: "Coolant" }
    const doc = { board, ...toPinsJson(MOCK_MAP, names) }
    const back = fromPinsJson(doc, fns)
    expect(back.map).toEqual(MOCK_MAP)
    expect(back.names).toEqual(names)
  })

  it("sends an empty encoder type for non-encoders", () => {
    const probe = toPinsJson(MOCK_MAP, {}).functions.find((f) => f.id === "probe")
    expect(probe?.encoder).toBe("")
  })
})
