import { describe, expect, it } from "vitest"
import {
  axisFields,
  configFields,
  inUnits,
  MACHINE_CONFIG_FIELDS,
  MACHINE_CONFIG_SECTIONS,
  parseMachineConfig,
  sectionsForCapabilities,
  sectionsInUnits,
  toDisplayValue,
  toRawValue,
  type MachineConfigField,
} from "./machine-config"

function field(key: string): MachineConfigField {
  const found = MACHINE_CONFIG_FIELDS.find((f) => f.key === key)
  if (!found) {
    throw new Error(`no field ${key}`)
  }
  return found
}

describe("machine config fields", () => {
  it("have unique keys", () => {
    const keys = MACHINE_CONFIG_FIELDS.map((f) => f.key)
    expect(new Set(keys).size).toBe(keys.length)
  })

  it("pair Z and X fields of the same setting", () => {
    for (const p of MACHINE_CONFIG_SECTIONS.flatMap((s) => s.pairs ?? [])) {
      expect(p.z.key.endsWith("Z"), p.z.key).toBe(true)
      expect(p.x.key).toBe(`${p.z.key.slice(0, -1)}X`)
    }
  })

  it("show every axis field", () => {
    const keys = MACHINE_CONFIG_FIELDS.map((f) => f.key)
    for (const f of [...axisFields("Z"), ...axisFields("X")]) {
      expect(keys).toContain(f.key)
    }
  })

  it("have a valid range unless a checkbox or a select", () => {
    for (const f of MACHINE_CONFIG_FIELDS) {
      if (f.type) {
        continue
      }
      expect(f.min, f.key).toBeLessThan(f.max!)
      expect(f.step, f.key).toBeGreaterThan(0)
    }
  })
})

describe("parseMachineConfig", () => {
  it("turns every value into text", () => {
    expect(parseMachineConfig({ encoderPpr: 600, invertEncoder: true, handwheelPprZ: 100.5 })).toEqual({
      encoderPpr: "600",
      invertEncoder: "true",
      handwheelPprZ: "100.5",
    })
  })

  it("accepts no data", () => {
    expect(parseMachineConfig(null)).toEqual({})
    expect(parseMachineConfig(undefined)).toEqual({})
  })
})

describe("display and raw values", () => {
  it("scales scaled fields", () => {
    const pitch = field("screwPitchDuZ")
    expect(toDisplayValue(pitch, "20000")).toBe(2)
    expect(toRawValue(pitch, 2)).toBe("20000")
    expect(toRawValue(pitch, 1.27)).toBe("12700")
    expect(toRawValue(pitch, 0.00014)).toBe("1")
  })

  it("passes unscaled fields through", () => {
    const steps = field("motorStepsZ")
    expect(toDisplayValue(steps, "800")).toBe(800)
    expect(toRawValue(steps, 800)).toBe("800")
    expect(toDisplayValue(steps, undefined)).toBe(0)
  })

  it("maps checkboxes", () => {
    const invert = field("invertEncoder")
    expect(toDisplayValue(invert, "true")).toBe(true)
    expect(toDisplayValue(invert, "1")).toBe(true)
    expect(toDisplayValue(invert, "false")).toBe(false)
    expect(toDisplayValue(invert, undefined)).toBe(false)
    expect(toRawValue(invert, true)).toBe("1")
    expect(toRawValue(invert, false)).toBe("0")
  })

  it("maps selects to their option values", () => {
    const boot = field("bootState")
    expect(boot.options!.map((o) => o.value)).toEqual([0, 1, 2])
    expect(toDisplayValue(boot, "2")).toBe(2)
    expect(toRawValue(boot, 1)).toBe("1")
  })

  it("round-trips every scaled field at its limits", () => {
    for (const f of MACHINE_CONFIG_FIELDS.filter((x) => x.scale)) {
      for (const display of [f.min!, f.max!]) {
        expect(toDisplayValue(f, toRawValue(f, display)), f.key).toBeCloseTo(display, 4)
      }
    }
  })
})

describe("speeds and accelerations", () => {
  it("show mm/min and mm/s²", () => {
    const rapid = field("speedRapidDuPerMinZ")
    expect(rapid.unit).toBe("mm/min")
    expect(toDisplayValue(rapid, "9600000")).toBe(960)
    expect(toRawValue(rapid, 960)).toBe("9600000")
    const accel = field("accelerationDuPerSec2X")
    expect(accel.unit).toBe("mm/s²")
    expect(toDisplayValue(accel, "125000")).toBe(12.5)
  })

  it("cover every speed and acceleration setting", () => {
    const keys = MACHINE_CONFIG_FIELDS.filter((f) => f.rate).map((f) => f.key)
    for (const axis of ["Z", "X"]) {
      for (const name of ["speedStart", "speedRapid"]) {
        expect(keys).toContain(`${name}DuPerMin${axis}`)
      }
      for (const name of ["acceleration", "deceleration"]) {
        expect(keys).toContain(`${name}DuPerSec2${axis}`)
      }
    }
    for (const axis of ["Z", "X"]) {
      expect(keys).toContain(`homeFastDuPerMin${axis}`)
      expect(keys).toContain(`homeSlowDuPerMin${axis}`)
    }
    expect(keys).toContain("probeFeedDuPerMin")
    expect(keys).toContain("reprobeFeedDuPerMin")
  })

  it("show in/min and in/s² with inch units", () => {
    const rapid = inUnits(field("speedRapidDuPerMinZ"), "inch")
    expect(rapid.unit).toBe("in/min")
    expect(toDisplayValue(rapid, "254000")).toBe(1)
    expect(toRawValue(rapid, 1)).toBe("254000")
    expect(inUnits(field("decelerationDuPerSec2Z"), "inch").unit).toBe("in/s²")
  })

  it("keep every other field with inch units", () => {
    const pitch = field("screwPitchDuZ")
    expect(inUnits(pitch, "inch")).toBe(pitch)
    const rapid = field("speedRapidDuPerMinZ")
    expect(inUnits(rapid, "mm")).toBe(rapid)
  })

  it("keep the keys of the sections with inch units", () => {
    expect(configFields(sectionsInUnits("inch")).map((f) => f.key)).toEqual(MACHINE_CONFIG_FIELDS.map((f) => f.key))
  })

  it("round-trip at the limits with inch units", () => {
    for (const f of configFields(sectionsInUnits("inch")).filter((x) => x.rate)) {
      for (const display of [f.min!, f.max!]) {
        expect(toDisplayValue(f, toRawValue(f, display)), f.key).toBeCloseTo(display, 4)
      }
    }
  })

  it("keep the same range in du with inch units", () => {
    for (const f of MACHINE_CONFIG_FIELDS.filter((x) => x.rate)) {
      const inch = inUnits(f, "inch")
      expect(Number(toRawValue(inch, inch.max!)), f.key).toBe(Number(toRawValue(f, f.max!)))
    }
  })
})

describe("sections without end stops", () => {
  const keys = (hasEndstops: boolean, units: "mm" | "inch" = "mm") =>
    configFields(sectionsForCapabilities(sectionsInUnits(units), hasEndstops)).map((f) => f.key)

  it("hide the max travel, the homing fields and require homing", () => {
    const hidden = keys(false)
    for (const key of ["maxTravelMmZ", "maxTravelMmX", "requireHoming", "homeFastDuPerMinZ", "homePositionDuX"]) {
      expect(hidden).not.toContain(key)
    }
    expect(hidden).toContain("speedRapidDuPerMinZ")
    expect(hidden).toContain("probeFeedDuPerMin")
  })

  it("keep every field with end stops", () => {
    expect(keys(true)).toEqual(MACHINE_CONFIG_FIELDS.map((f) => f.key))
    expect(keys(true, "inch")).toEqual(MACHINE_CONFIG_FIELDS.map((f) => f.key))
  })
})
