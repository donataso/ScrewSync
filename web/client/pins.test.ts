import { describe, expect, it } from "vitest"
import {
  CONNECTOR_NAME_MAX,
  FUNCTIONS,
  MOCK_BOARD,
  MOCK_MAP,
  canStartAt,
  connectorLabel,
  defaultAssignment,
  functionDef,
  functionsAt,
  nameIssues,
  pinsOf,
  placements,
  validate,
  type PinMap,
} from "./pins"

const fn = (id: string) => functionDef(id)!

describe("pinsOf", () => {
  it("gives the GPIOs in signal order", () => {
    expect(pinsOf(MOCK_BOARD, fn("zMotor"), MOCK_MAP.zMotor)).toEqual([16, 15, 7])
  })

  it("gives null for power pins", () => {
    expect(pinsOf(MOCK_BOARD, fn("probe"), { ...MOCK_MAP.probe, start: 0 })).toEqual([null])
  })
})

describe("validate", () => {
  it("accepts the default map, with strapping warnings only", () => {
    const issues = validate(MOCK_BOARD, MOCK_MAP)
    expect(issues.filter((i) => i.level === "error")).toEqual([])
    expect(issues.filter((i) => i.level === "warn").map((i) => i.gpio)).toEqual([3, 46])
  })

  it("rejects a missing required function", () => {
    const map: PinMap = { ...MOCK_MAP }
    delete map.zMotor
    expect(validate(MOCK_BOARD, map).some((i) => i.level === "error" && i.fn === "zMotor")).toBe(true)
  })

  it("accepts a missing optional function", () => {
    const map: PinMap = { ...MOCK_MAP }
    delete map.xMotor
    expect(validate(MOCK_BOARD, map).filter((i) => i.level === "error")).toEqual([])
  })

  it("rejects a GPIO used twice", () => {
    const map: PinMap = { ...MOCK_MAP, probe: { ...MOCK_MAP.probe, connector: "r3c5", start: 2 } }
    expect(validate(MOCK_BOARD, map).some((i) => i.level === "error" && i.gpio === 10)).toBe(true)
  })

  it("rejects a reserved pin", () => {
    const map: PinMap = { ...MOCK_MAP, probe: { ...MOCK_MAP.probe, connector: "r1c1", start: 3 } }
    expect(validate(MOCK_BOARD, map).some((i) => i.level === "error" && i.gpio === 35)).toBe(true)
  })

  it("rejects a group that runs onto a power pin", () => {
    const map: PinMap = { ...MOCK_MAP, xMotor: { ...MOCK_MAP.xMotor, connector: "r2c5", start: 1 } }
    expect(validate(MOCK_BOARD, map).some((i) => i.level === "error" && i.fn === "xMotor")).toBe(true)
  })

  it("rejects more hardware encoders than PCNT units", () => {
    const board = { ...MOCK_BOARD, pcntUnits: 2 }
    expect(validate(board, MOCK_MAP).some((i) => i.level === "error" && i.msg.includes("PCNT"))).toBe(true)
  })

  it("rejects an encoder type the function does not allow", () => {
    const map: PinMap = { ...MOCK_MAP, spindle: { ...MOCK_MAP.spindle, encoderType: "sw" } }
    expect(validate(MOCK_BOARD, map).some((i) => i.level === "error" && i.fn === "spindle")).toBe(true)
  })
})

describe("placement", () => {
  it("allows a group only on free GPIOs next to each other", () => {
    expect(canStartAt(MOCK_BOARD, MOCK_MAP, "xMotor", "r1c4", 0)).toBe(true)
    expect(canStartAt(MOCK_BOARD, MOCK_MAP, "xMotor", "r1c4", 2)).toBe(false)
    expect(canStartAt(MOCK_BOARD, MOCK_MAP, "xMotor", "r1c2", 1)).toBe(false)
    expect(canStartAt(MOCK_BOARD, MOCK_MAP, "xMotor", "r1c1", 1)).toBe(false)
  })

  it("allows a function on its own pins", () => {
    expect(canStartAt(MOCK_BOARD, MOCK_MAP, "zMotor", "r1c2", 1)).toBe(true)
  })

  it("lists every place a group fits", () => {
    const map: PinMap = { ...MOCK_MAP }
    delete map.xMotor
    expect(placements(MOCK_BOARD, map, "xMotor")).toEqual([
      { connector: "r1c3", start: 1 },
      { connector: "r1c4", start: 0 },
      { connector: "r1c4", start: 1 },
    ])
  })

  it("lists the functions that can start at a pin", () => {
    expect(functionsAt(MOCK_BOARD, MOCK_MAP, "r3c2", 2).map((f) => f.id)).toEqual(["spindle", "zHandwheel", "xHandwheel", "probe"])
  })

  it("gives inputs a pull-up by default", () => {
    expect(defaultAssignment(fn("probe"), "r3c2", 2).options).toEqual([{ invert: false, pull: "up" }])
    expect(FUNCTIONS.every((f) => !f.encoderTypes || defaultAssignment(f, "r3c2", 2).encoderType === f.encoderTypes[0])).toBe(true)
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
