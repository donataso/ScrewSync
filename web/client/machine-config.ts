export interface MachineConfigField {
  key: string
  label: string
  help: string
  unit?: string
  min?: number
  max?: number
  step?: number
  scale?: number
  type?: "checkbox"
}

export interface MachineConfigPair {
  z: MachineConfigField
  x: MachineConfigField
  copy: boolean
}

export interface MachineConfigSection {
  title: string
  fields: MachineConfigField[]
  pairs?: MachineConfigPair[]
}

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
      key: `speedStart${axis}`,
      label: "Start speed",
      unit: "steps/s",
      min: 1,
      max: 1000000,
      step: 1,
      help: "Initial step pulse rate when motion starts. Use a value the motor can start from reliably without missing steps.",
    },
    {
      key: `speedManualMove${axis}`,
      label: "Manual speed",
      unit: "steps/s",
      min: 1,
      max: 1000000,
      step: 1,
      help: `Top speed for jogs and manual moves on ${axis}. Axis speed depends on both this value and steps per mm.`,
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
      key: `acceleration${axis}`,
      label: "Acceleration",
      unit: "steps/s^2",
      min: 1,
      max: 100000000,
      step: 1,
      help: `Ramp rate for speeding up. Higher feels snappier but can skip steps if the motor or ${a} cannot keep up.`,
    },
    {
      key: `deceleration${axis}`,
      label: "Deceleration",
      unit: "steps/s^2",
      min: 1,
      max: 100000000,
      step: 1,
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
      label: "Handwheel PPR",
      unit: "pulses/rev",
      min: 1,
      max: 100000,
      step: 0.01,
      help: `Physical pulses per revolution of the optional ${axis} handwheel encoder. Use the handwheel specification.`,
    },
    {
      key: `handwheelLagMs${axis}`,
      label: "Handwheel lag",
      unit: "ms",
      min: 1,
      max: 1000,
      step: 1,
      help: `How far ${axis} may fall behind a fast handwheel, as time at the manual move speed. Turning beyond that is dropped, so ${axis} stops soon after the handwheel stops.`,
    },
    {
      key: `invertHandwheel${axis}`,
      label: "Invert handwheel direction",
      type: "checkbox",
      help: `Toggle if turning the ${axis} handwheel moves the axis the wrong way.`,
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

function pair(name: string, copy: boolean): MachineConfigPair {
  const find = (fields: MachineConfigField[], key: string) => {
    const f = fields.find((x) => x.key === key)
    if (!f) {
      throw new Error(`no field ${key}`)
    }
    return f
  }
  return { z: find(Z_FIELDS, `${name}Z`), x: find(X_FIELDS, `${name}X`), copy }
}

export const MACHINE_CONFIG_SECTIONS: MachineConfigSection[] = [
  {
    title: "Encoders",
    fields: [
      {
        key: "encoderPpr",
        label: "Spindle encoder PPR",
        unit: "pulses/rev",
        min: 1,
        max: 15000,
        step: 1,
        help: "Pulses per spindle revolution from the encoder specification before quadrature counting. Common values are 600 or 1200.",
      },
      {
        key: "encoderBacklash",
        label: "Spindle encoder backlash",
        unit: "pulses",
        min: 0,
        max: 30000,
        step: 1,
        help: "Encoder pulses ignored after spindle direction reverses. Increase only if the displayed angle flickers after reversal.",
      },
      {
        key: "invertEncoder",
        label: "Invert spindle encoder",
        type: "checkbox",
        help: "Toggle if the spindle position/RPM sign is reversed from the actual spindle rotation direction.",
      },
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
    pairs: [pair("handwheelPpr", true), pair("handwheelLagMs", true), pair("invertHandwheel", true)],
  },
  {
    title: "Drive",
    fields: [],
    pairs: [
      pair("screwPitchDu", false),
      pair("motorSteps", true),
      pair("backlashDu", false),
      pair("maxTravelMm", false),
      pair("invertDirection", false),
      pair("invertEnable", true),
    ],
  },
  {
    title: "Motion",
    fields: [],
    pairs: [pair("speedStart", true), pair("speedManualMove", true), pair("acceleration", true), pair("deceleration", true)],
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
        key: "probePolarityNc",
        label: "Normally closed",
        type: "checkbox",
        help: "Toggle for a normally-closed probe (circuit closed until it triggers). Off for normally-open.",
      },
    ],
  },
]

export const MACHINE_CONFIG_FIELDS: MachineConfigField[] = MACHINE_CONFIG_SECTIONS.flatMap((s) => [
  ...s.fields,
  ...(s.pairs ?? []).flatMap((p) => [p.z, p.x]),
])

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
