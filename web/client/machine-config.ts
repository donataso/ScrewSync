export interface MachineConfigField {
  key: string
  label: string
  help: string
  unit?: string
  min?: number
  max?: number
  step?: number
  scale?: number
  /** A speed or acceleration: shown in the machine units. */
  rate?: "speed" | "accel"
  type?: "checkbox"
}

export interface MachineConfigPair {
  z: MachineConfigField
  x: MachineConfigField
  copy: boolean
  needsEndstops?: boolean
}

export interface MachineConfigSection {
  title: string
  fieldsTitle?: string
  fields: MachineConfigField[]
  /** Shown with the pairs, for both axes. */
  common?: MachineConfigField[]
  commonTitle?: string
  pairs?: MachineConfigPair[]
  pairsTitle?: string
  needsEndstops?: boolean
}

const DU_PER_MM = 10000
const DU_PER_INCH = 254000
const INCH_STEP = 0.00001

export function axisFields(axis: "Z" | "X"): MachineConfigField[] {
  const a = axis === "Z" ? "the carriage" : "the cross-slide"
  return [
    {
      key: `screwPitchDu${axis}`,
      label: "Lead screw pitch",
      unit: "mm/rev",
      min: 0.0001,
      max: 1000,
      step: 0.0001,
      scale: 10000,
      help: `Distance ${axis} moves for one lead screw revolution. Metric screw: use the marked pitch. Inch screw: 25.4 / TPI.`,
    },
    {
      key: `motorSteps${axis}`,
      label: "Motor steps per rev",
      unit: "steps/rev",
      min: 1,
      max: 1000000,
      step: 1,
      help: "Full motor steps * driver microsteps * motor-to-screw ratio. Example: 200-step motor at 4x microstepping = 800.",
    },
    {
      key: `backlashDu${axis}`,
      label: "Backlash",
      unit: "mm",
      min: 0,
      max: 1000,
      step: 0.0001,
      scale: 10000,
      help: `Lost ${axis} motion when reversing direction. Measure with a dial indicator: move one way, zero it, reverse until the indicator starts moving.`,
    },
    {
      key: `speedStartDuPerMin${axis}`,
      label: "Start speed",
      unit: "mm/min",
      min: 0.0001,
      max: 6000,
      step: 0.0001,
      scale: DU_PER_MM,
      rate: "speed",
      help: "Initial speed when motion starts. Use a value the motor can start from reliably without missing steps.",
    },
    {
      key: `speedRapidDuPerMin${axis}`,
      label: "Rapid speed",
      unit: "mm/min",
      min: 0.0001,
      max: 6000,
      step: 0.0001,
      scale: DU_PER_MM,
      rate: "speed",
      help: `Top speed for rapids, jogs and manual moves on ${axis}.`,
    },
    {
      key: `maxTravelMm${axis}`,
      label: "Max travel",
      unit: "mm",
      min: 1,
      max: 10000,
      step: 1,
      help: `Maximum single ${axis} move the controller will allow. Set a little above the physically safe travel of ${a}.`,
    },
    {
      key: `accelerationDuPerSec2${axis}`,
      label: "Acceleration",
      unit: "mm/s²",
      min: 0.0001,
      max: 10000,
      step: 0.0001,
      scale: DU_PER_MM,
      rate: "accel",
      help: `Ramp rate for speeding up. Higher feels snappier but can skip steps if the motor or ${a} cannot keep up.`,
    },
    {
      key: `decelerationDuPerSec2${axis}`,
      label: "Deceleration",
      unit: "mm/s²",
      min: 0.0001,
      max: 10000,
      step: 0.0001,
      scale: DU_PER_MM,
      rate: "accel",
      help: `Ramp rate for slowing down. Friction helps the motor stop, so this can be higher than acceleration.`,
    },
    {
      key: `invertDirection${axis}`,
      label: "Invert direction",
      type: "checkbox",
      help: `Toggle if ${axis} moves opposite to the expected direction.`,
    },
    {
      key: `handwheelPpr${axis}`,
      label: "PPR",
      unit: "pulses/rev",
      min: 1,
      max: 100000,
      step: 0.01,
      help: `Physical pulses per revolution of the optional ${axis} handwheel encoder. Use the handwheel specification.`,
    },
    {
      key: `handwheelLagMs${axis}`,
      label: "Lag",
      unit: "ms",
      min: 1,
      max: 1000,
      step: 1,
      help: `How far ${axis} may fall behind a fast handwheel, as time at the rapid speed. Turning beyond that is dropped, so ${axis} stops soon after the handwheel stops.`,
    },
    {
      key: `invertHandwheel${axis}`,
      label: "Invert direction",
      type: "checkbox",
      help: `Toggle if turning the ${axis} handwheel moves the axis the wrong way.`,
    },
    {
      key: `homeFastDuPerMin${axis}`,
      label: "Home fast speed",
      unit: "mm/min",
      min: 0,
      max: 6000,
      step: 0.0001,
      scale: DU_PER_MM,
      rate: "speed",
      help: `Speed of the first pass of ${axis} toward the home switch.`,
    },
    {
      key: `homeSlowDuPerMin${axis}`,
      label: "Home slow speed",
      unit: "mm/min",
      min: 0,
      max: 6000,
      step: 0.0001,
      scale: DU_PER_MM,
      rate: "speed",
      help: `Speed of the second pass of ${axis} toward the home switch.`,
    },
    {
      key: `homeLatchBackOffDu${axis}`,
      label: "Home latch back-off",
      unit: "mm",
      min: 1,
      max: 1000,
      step: 0.0001,
      scale: DU_PER_MM,
      help: `Distance ${axis} backs off after the fast pass, before the slow pass.`,
    },
    {
      key: `homePullOffDu${axis}`,
      label: "Home pull-off",
      unit: "mm",
      min: 0,
      max: 1000,
      step: 0.0001,
      scale: DU_PER_MM,
      help: `Distance ${axis} moves off the switch after the slow pass.`,
    },
    {
      key: `homePositionDu${axis}`,
      label: "Home position",
      unit: "mm",
      min: -1000,
      max: 1000,
      step: 0.0001,
      scale: DU_PER_MM,
      help: `Machine coordinate of ${axis} after the pull-off.`,
    },
    {
      key: `invertEnable${axis}`,
      label: "Invert enable",
      type: "checkbox",
      help: "Toggle only when the stepper driver's enable pin polarity is inverted.",
    },
  ]
}

const Z_FIELDS = axisFields("Z")
const X_FIELDS = axisFields("X")

function pair(name: string, copy: boolean, needsEndstops = false): MachineConfigPair {
  const find = (fields: MachineConfigField[], key: string) => {
    const f = fields.find((x) => x.key === key)
    if (!f) {
      throw new Error(`no field ${key}`)
    }
    return f
  }
  return { z: find(Z_FIELDS, `${name}Z`), x: find(X_FIELDS, `${name}X`), copy, needsEndstops }
}

export const MACHINE_CONFIG_SECTIONS: MachineConfigSection[] = [
  {
    title: "Encoders",
    fieldsTitle: "Spindle encoder",
    fields: [
      {
        key: "encoderPpr",
        label: "PPR",
        unit: "pulses/rev",
        min: 1,
        max: 15000,
        step: 1,
        help: "Pulses per spindle revolution from the encoder specification before quadrature counting. Common values are 600 or 1200.",
      },
      {
        key: "encoderBacklash",
        label: "Backlash",
        unit: "pulses",
        min: 0,
        max: 30000,
        step: 1,
        help: "Encoder pulses ignored after spindle direction reverses. Increase only if the displayed angle flickers after reversal.",
      },
      {
        key: "invertEncoder",
        label: "Invert direction",
        type: "checkbox",
        help: "Toggle if the spindle position/RPM sign is reversed from the actual spindle rotation direction.",
      },
    ],
    commonTitle: "Handwheel encoders",
    common: [
      {
        key: "axisEncoderBacklash",
        label: "Handwheel backlash",
        unit: "pulses",
        min: 0,
        max: 30000,
        step: 1,
        help: "Handwheel encoder pulses ignored after direction reverses. Set to 0 for direct encoders without mechanical play.",
      },
    ],
    pairsTitle: "handwheel",
    pairs: [pair("handwheelPpr", true), pair("handwheelLagMs", true), pair("invertHandwheel", false)],
  },
  {
    title: "Drive",
    fields: [],
    pairs: [
      pair("screwPitchDu", false),
      pair("motorSteps", true),
      pair("backlashDu", false),
      pair("maxTravelMm", false, true),
      pair("invertDirection", false),
      pair("invertEnable", false),
    ],
  },
  {
    title: "Motion",
    fields: [],
    pairs: [
      pair("speedStartDuPerMin", true),
      pair("speedRapidDuPerMin", true),
      pair("accelerationDuPerSec2", true),
      pair("decelerationDuPerSec2", true),
    ],
  },
  {
    title: "Homing",
    needsEndstops: true,
    fields: [
      {
        key: "requireHoming",
        label: "Require homing",
        type: "checkbox",
        help: "Block everything except homing, estop off, settings and motor enable until the axes are homed.",
      },
    ],
    pairs: [
      pair("homeFastDuPerMin", true),
      pair("homeSlowDuPerMin", true),
      pair("homeLatchBackOffDu", true),
      pair("homePullOffDu", true),
      pair("homePositionDu", false),
    ],
  },
  {
    title: "Manual movement",
    fields: [
      {
        key: "stepTimeMs",
        label: "Step time",
        unit: "ms",
        min: 1,
        max: 10000,
        step: 1,
        help: "Target time for one precision button step when the selected move step is not continuous.",
      },
      {
        key: "delayBetweenStepsMs",
        label: "Step delay",
        unit: "ms",
        min: 0,
        max: 10000,
        step: 1,
        help: "Pause between repeated precision steps while a manual move button is held.",
      },
    ],
  },
  {
    title: "Automated modes",
    fields: [
      {
        key: "safeDistanceDu",
        label: "Safe retract distance",
        unit: "mm",
        min: 0,
        max: 1000,
        step: 0.0001,
        scale: 10000,
        help: "Distance the auxiliary axis backs away from the material between automated passes. Set to 0 to disable the extra retract.",
      },
      {
        key: "slotLeftReductionDu",
        label: "Slot left reduction",
        unit: "mm/pass",
        min: 0,
        max: 1000,
        step: 0.0001,
        scale: 10000,
        help: "Shortens each successive slotting pass by this Z distance for blind slots. Set to 0 for full-length passes.",
      },
    ],
  },
  {
    title: "Probe",
    fields: [
      {
        key: "probeFeedDuPerMin",
        label: "Probe feed",
        unit: "mm/min",
        min: 0.0001,
        max: 6000,
        step: 0.0001,
        scale: 10000,
        rate: "speed",
        help: "Feed rate for the first touch. Keep slow so the probe stops within its travel after it triggers.",
      },
      {
        key: "reprobeFeedDuPerMin",
        label: "Reprobe feed",
        unit: "mm/min",
        min: 0.0001,
        max: 6000,
        step: 0.0001,
        scale: 10000,
        rate: "speed",
        help: "Feed rate for the back-offs and the reprobe. Slower than the probe feed for a more accurate second touch.",
      },
      {
        key: "probeBackOffDu",
        label: "Back off",
        unit: "mm",
        min: 0.0001,
        max: 1000,
        step: 0.0001,
        scale: 10000,
        help: "Retract after each touch.",
      },
      {
        key: "probeDstDu",
        label: "Distance",
        unit: "mm",
        min: 0,
        max: 1000,
        step: 0.0001,
        scale: 10000,
        help: "The furthest the first touch may travel. 0 = not set: the probe routine refuses to start.",
      },
      {
        key: "probeOffsetDu",
        label: "Offset",
        unit: "mm",
        min: 0,
        max: 1000,
        step: 0.0001,
        scale: 10000,
        help: "Trigger point to workpiece surface: plate thickness, or measured once for a switch.",
      },
      {
        key: "probePolarityNc",
        label: "Normally closed",
        type: "checkbox",
        help: "Toggle for a normally-closed probe (circuit closed until it triggers). Off for normally-open.",
      },
    ],
  },
]

export function configFields(sections: MachineConfigSection[]): MachineConfigField[] {
  return sections.flatMap((s) => [...s.fields, ...(s.common ?? []), ...(s.pairs ?? []).flatMap((p) => [p.z, p.x])])
}

export const MACHINE_CONFIG_FIELDS: MachineConfigField[] = configFields(MACHINE_CONFIG_SECTIONS)

export function inUnits(field: MachineConfigField, units: "mm" | "inch"): MachineConfigField {
  if (!field.rate || units !== "inch") {
    return field
  }
  return {
    ...field,
    unit: field.rate === "speed" ? "in/min" : "in/s²",
    min: INCH_STEP,
    max: (field.max! * field.scale!) / DU_PER_INCH,
    step: INCH_STEP,
    scale: DU_PER_INCH,
  }
}

export function sectionsInUnits(units: "mm" | "inch"): MachineConfigSection[] {
  const convert = (f: MachineConfigField) => inUnits(f, units)
  return MACHINE_CONFIG_SECTIONS.map((s) => ({
    ...s,
    fields: s.fields.map(convert),
    common: s.common?.map(convert),
    pairs: s.pairs?.map((p) => ({ ...p, z: convert(p.z), x: convert(p.x) })),
  }))
}

export function parseMachineConfig(data: unknown): Record<string, string> {
  const out: Record<string, string> = {}
  for (const [key, value] of Object.entries((data ?? {}) as Record<string, unknown>)) {
out[key] = String(value)
}
  return out
}

export function toDisplayValue(field: MachineConfigField, raw: string | undefined): number | boolean {
  if (field.type === "checkbox") {
return raw === "true" || raw === "1"
}
  const n = Number(raw ?? 0)
  return field.scale ? n / field.scale : n
}

export function toRawValue(field: MachineConfigField, display: number | boolean): string {
  if (field.type === "checkbox") {
return display ? "1" : "0"
}
  const n = display as number
  return String(field.scale ? Math.round(n * field.scale) : n)
}

export function sectionsForCapabilities(sections: MachineConfigSection[], hasEndstops: boolean): MachineConfigSection[] {
  if (hasEndstops) {
    return sections
  }
  return sections.filter((s) => !s.needsEndstops).map((s) => ({ ...s, pairs: s.pairs?.filter((p) => !p.needsEndstops) }))
}
