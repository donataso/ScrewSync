import { describe, expect, it } from "vitest"
import {
  DESIGNATION_MAX_LEN,
  MAX_GROUP_ENTRIES,
  MAX_GROUPS,
  NAME_MAX_LEN,
  degreesToRatio,
  factorError,
  flankAngleError,
  formatDegrees,
  formatLength,
  formatRatioN,
  formatTpi,
  fromJson,
  groupDepthDu,
  groupErrors,
  hasOverride,
  lengthUnit,
  nameError,
  newGroupName,
  parseLength,
  parseTpi,
  ratioToDegrees,
  tableValid,
  taperError,
  toJson,
  type ThreadGroup,
  type ThreadTableJson,
} from "./threads"

const TABLE: ThreadTableJson = {
  groups: [
    { name: "metric", depthExternal: 0.61343, depthInternal: 0.54127, taper: 0, flankAngle: 60 },
    { name: "npt", depthExternal: 0.8, depthInternal: 0.8, taper: 0.0625, flankAngle: 60 },
  ],
  entries: [
    { group: "metric", designation: "M10", majorDu: 100000, pitchDu: 15000 },
    { group: "metric", designation: "M10x1.25", majorDu: 100000, pitchDu: 12500 },
    { group: "npt", designation: "1/2NPT", majorDu: 213360, pitchDu: 18143 },
  ],
  overrides: [
    { group: "metric", designation: "M10", depthExternalDu: 9000, depthInternalDu: 0, hasTaper: false, taper: 0 },
    { group: "npt", designation: "1/2NPT", depthExternalDu: 0, depthInternalDu: 0, hasTaper: true, taper: 0.05 },
  ],
}

function group(over: Partial<ThreadGroup> = {}): ThreadGroup {
  return { name: "metric", depthExternal: "0.6", depthInternal: "0.5", taper: 0, flankAngle: "60", entries: [], ...over }
}

describe("fromJson / toJson", () => {
  it("puts entries and overrides into their groups", () => {
    const groups = fromJson(TABLE)
    expect(groups.map((g) => g.name)).toEqual(["metric", "npt"])
    expect(groups[0].entries.map((e) => e.designation)).toEqual(["M10", "M10x1.25"])
    expect(groups[0].entries[0].depthExternalDu).toBe(9000)
    expect(groups[0].entries[0].depthInternalDu).toBeUndefined()
    expect(groups[0].entries[0].taper).toBeUndefined()
    expect(groups[1].entries[0].taper).toBe(0.05)
    expect(hasOverride(groups[0].entries[0])).toBe(true)
    expect(hasOverride(groups[0].entries[1])).toBe(false)
  })

  it("round-trips", () => {
    expect(toJson(fromJson(TABLE))).toEqual(TABLE)
  })

  it("drops entries and overrides without a match", () => {
    const groups = fromJson({
      groups: TABLE.groups,
      entries: [{ group: "nope", designation: "M1", majorDu: 10000, pitchDu: 2500 }],
      overrides: [{ group: "metric", designation: "M1", depthExternalDu: 1, depthInternalDu: 1, hasTaper: true, taper: 0.1 }],
    })
    expect(groups.every((g) => g.entries.length === 0)).toBe(true)
  })

  it("accepts a table without overrides", () => {
    const groups = fromJson({ groups: TABLE.groups, entries: TABLE.entries })
    expect(groups[0].entries.some(hasOverride)).toBe(false)
  })
})

describe("groupDepthDu", () => {
  it("scales the pitch magnitude by the side's factor", () => {
    const g = group({ depthExternal: "0.61343", depthInternal: "0.54127" })
    expect(groupDepthDu(g, 15000, false)).toBe(9201)
    expect(groupDepthDu(g, -15000, true)).toBe(8119)
  })
})

describe("lengths", () => {
  it("formats in the selected units without trailing zeros", () => {
    expect(formatLength(15000, "mm")).toBe("1.5")
    expect(formatLength(100000, "mm")).toBe("10")
    expect(formatLength(254000, "inch")).toBe("1")
    expect(formatLength(12700, "inch")).toBe("0.05")
    expect(lengthUnit("mm")).toBe("mm")
    expect(lengthUnit("inch")).toBe("in")
  })

  it("parses to deci-microns", () => {
    expect(parseLength("1.5", "mm")).toBe(15000)
    expect(parseLength("0.5", "inch")).toBe(127000)
    expect(parseLength("  ", "mm")).toBeNaN()
    expect(parseLength("abc", "mm")).toBeNaN()
  })
})

describe("tpi", () => {
  it("converts both ways", () => {
    expect(parseTpi("20")).toBe(12700)
    expect(formatTpi(12700)).toBe("20")
    expect(formatTpi(parseTpi("4.5"))).toBe("4.5")
  })

  it("rejects empty, zero, negative and non-numeric text", () => {
    for (const text of ["", " ", "0", "-8", "abc"]) {
      expect(parseTpi(text)).toBeNaN()
    }
  })
})

describe("taper conversion", () => {
  it("is the half-angle of the diameter ratio", () => {
    expect(ratioToDegrees(2)).toBeCloseTo(45)
    expect(degreesToRatio(45)).toBeCloseTo(2)
    expect(ratioToDegrees(degreesToRatio(1.7899))).toBeCloseTo(1.7899)
  })

  it("formats the magnitude", () => {
    expect(formatDegrees(-0.0625)).toBe("1.7899")
    expect(formatRatioN(0.0625)).toBe("16")
    expect(formatRatioN(-0.0625)).toBe("16")
    expect(formatRatioN(0)).toBe("")
  })
})

describe("validation", () => {
  it("checks names", () => {
    expect(nameError("metric", [], "Group", NAME_MAX_LEN)).toBeUndefined()
    expect(nameError("", [], "Group", NAME_MAX_LEN)).toBe("Group is empty")
    expect(nameError("a".repeat(NAME_MAX_LEN), [], "Group", NAME_MAX_LEN)).toBeUndefined()
    expect(nameError("a".repeat(NAME_MAX_LEN + 1), [], "Group", NAME_MAX_LEN)).toBeDefined()
    expect(nameError("a".repeat(DESIGNATION_MAX_LEN + 1), [], "Thread", DESIGNATION_MAX_LEN)).toBeDefined()
    expect(nameError("a b", [], "Group", NAME_MAX_LEN)).toBeDefined()
    expect(nameError("a,b", [], "Group", NAME_MAX_LEN)).toBeDefined()
    expect(nameError("metric", ["metric"], "Group", NAME_MAX_LEN)).toBe("Group already exists")
  })

  it("checks depth factors", () => {
    expect(factorError("0.6")).toBeUndefined()
    for (const text of ["", "abc", "0", "-1", "2"]) {
      expect(factorError(text)).toBeDefined()
    }
  })

  it("checks flank angles", () => {
    expect(flankAngleError("55")).toBeUndefined()
    for (const text of ["", "abc", "0", "120"]) {
      expect(flankAngleError(text)).toBeDefined()
    }
  })

  it("checks tapers", () => {
    expect(taperError(undefined)).toBeUndefined()
    expect(taperError(0)).toBeUndefined()
    expect(taperError(-0.99)).toBeUndefined()
    expect(taperError(NaN)).toBe("Not a number")
    expect(taperError(1)).toBeDefined()
    expect(taperError(-1)).toBeDefined()
  })

  it("reports each bad field of a group", () => {
    const groups = [group(), group({ depthExternal: "9", flankAngle: "", taper: 2 })]
    expect(Object.values(groupErrors([group(), group({ name: "other" })], 0)).every((e) => !e)).toBe(true)
    const errors = groupErrors(groups, 1)
    expect(errors.name).toBe("Group already exists")
    expect(errors.depthExternal).toBeDefined()
    expect(errors.depthInternal).toBeUndefined()
    expect(errors.flankAngle).toBeDefined()
    expect(errors.taper).toBeDefined()
  })

  it("accepts a table only within the limits", () => {
    const entry = { designation: "M1", majorDu: 10000, pitchDu: 2500 }
    const full = Array.from({ length: MAX_GROUPS }, (_, i) => group({ name: `g${i}` }))
    expect(tableValid(fromJson(TABLE))).toBe(true)
    expect(tableValid(full)).toBe(true)
    expect(tableValid([...full, group({ name: "extra" })])).toBe(false)
    expect(tableValid([group({ entries: Array(MAX_GROUP_ENTRIES).fill(entry) })])).toBe(true)
    expect(tableValid([group({ entries: Array(MAX_GROUP_ENTRIES + 1).fill(entry) })])).toBe(false)
    expect(tableValid([group({ depthInternal: "x" })])).toBe(false)
  })
})

describe("newGroupName", () => {
  it("picks the first free name", () => {
    expect(newGroupName([])).toBe("new")
    expect(newGroupName([group({ name: "new" })])).toBe("new2")
    expect(newGroupName([group({ name: "new" }), group({ name: "new2" })])).toBe("new3")
  })
})
