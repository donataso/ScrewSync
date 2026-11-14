"use client"

import { useEffect, useRef, useState } from "react"
import { sendCommand } from "../api"
import { parseMachineConfig } from "../machine-config"
import { CONFIGURE_LOCK_MESSAGE, useCanConfigure, useDeviceState } from "../state-provider"
import type { CmdOutcome } from "../types"
import { Badge, Button, Field, LockNotice, Panel, TextInput } from "../ui/primitives"
import { useToast } from "../ui/toast"

const DU_PER_MM = 10000
const DIRECTION_MOVE_DU = DU_PER_MM
const PRELOAD_MOVE_DU = DU_PER_MM
const STEPS_PER_MM_DU_PER_MIN = 100 * DU_PER_MM
const BACKLASH_RUNS = 5
const BACKLASH_SIDE_TOLERANCE = 0.2

type Axis = "Z" | "X"
type Side = "-" | "+"
type Raw = Record<string, string>

function mm(du: number, digits = 4): string {
  return `${(du / DU_PER_MM).toFixed(digits)} mm`
}

function denial(outcome: CmdOutcome): string {
  return outcome.status === "denied" ? outcome.msg : "No reply from device."
}

function replyMsg(outcome: CmdOutcome): string {
  return outcome.status === "ok" ? String((outcome.data as { msg?: string })?.msg ?? "") : ""
}

function replyValue(outcome: CmdOutcome, key: string): number | null {
  const m = new RegExp(`${key}=(-?\\d+)`).exec(replyMsg(outcome))
  return m ? Number(m[1]) : null
}

export function CalibrationScreen() {
  const [raw, setRaw] = useState<Raw | null>(null)
  const canConfigure = useCanConfigure()
  const toast = useToast()

  async function load() {
    const outcome = await sendCommand("settings get")
    if (outcome.status === "ok") {
      setRaw(parseMachineConfig(outcome.data))
    } else {
      toast.push(denial(outcome), "bad")
    }
  }

  useEffect(() => {
    load()
    // eslint-disable-next-line react-hooks/exhaustive-deps
  }, [])

  async function apply(key: string, value: string) {
    const outcome = await sendCommand(`settings set ${key} ${value}`)
    if (outcome.status === "ok") {
      toast.push("Applied and saved to flash.", "good")
      await load()
    } else {
      toast.push(denial(outcome), "bad")
    }
  }

  const props = { raw, apply, canConfigure }
  return (
    <div className="flex flex-col gap-4">
      <h1 className="text-lg font-semibold">Calibration</h1>
      {!canConfigure && <LockNotice>{CONFIGURE_LOCK_MESSAGE}</LockNotice>}
      {!raw ? (
        <Panel>
          <p className="text-sm text-muted-foreground">Loading...</p>
        </Panel>
      ) : (
        <>
          <ProbePolarity {...props} />
          <SpindleDirection {...props} />
          <EncoderPpr {...props} />
          <AxisDirection {...props} />
          <StepsPerMm {...props} />
          <Backlash {...props} />
        </>
      )}
    </div>
  )
}

interface CheckProps {
  raw: Raw | null
  apply: (key: string, value: string) => Promise<void>
  canConfigure: boolean
}

function Suggestion({
  current,
  suggested,
  disabled,
  onApply,
}: {
  current: string
  suggested: string | null
  disabled: boolean
  onApply: () => void
}) {
  const same = suggested === null || suggested === current
  return (
    <div className="flex flex-wrap items-center gap-3 text-sm">
      <span className="text-muted-foreground">
        Current: <span className="text-foreground">{current}</span>
      </span>
      {suggested !== null && (
        <span className="text-muted-foreground">
          Measured: <span className="text-foreground">{suggested}</span>
        </span>
      )}
      {suggested !== null && same && <Badge tone="good">OK</Badge>}
      <Button variant="primary" disabled={disabled || same} onClick={onApply}>
        Apply
      </Button>
    </div>
  )
}

function useCountReader() {
  const toast = useToast()
  return async (): Promise<number | null> => {
    const outcome = await sendCommand("cal count")
    const count = replyValue(outcome, "count")
    if (count === null) {
      toast.push(denial(outcome), "bad")
    }
    return count
  }
}

function ProbePolarity({ raw, apply, canConfigure }: CheckProps) {
  const { state } = useDeviceState()
  const nc = raw?.probePolarityNc === "true"
  const contact = state?.probeContact ?? false
  const suggested = contact ? !nc : nc
  return (
    <Panel title="Probe polarity">
      <p className="mb-3 text-sm text-muted-foreground">Keep the probe open. The contact must show open.</p>
      <div className="mb-3 text-sm">
        Contact: <Badge tone={contact ? "warn" : "good"}>{contact ? "closed" : "open"}</Badge>
      </div>
      <Suggestion
        current={nc ? "NC" : "NO"}
        suggested={suggested ? "NC" : "NO"}
        disabled={!canConfigure}
        onApply={() => apply("probePolarityNc", String(suggested))}
      />
    </Panel>
  )
}

function useCountDelta() {
  const readCount = useCountReader()
  const [start, setStart] = useState<number | null>(null)
  const [delta, setDelta] = useState<number | null>(null)
  return {
    started: start !== null,
    delta,
    begin: async () => {
      setDelta(null)
      setStart(await readCount())
    },
    end: async () => {
      const now = await readCount()
      if (now !== null && start !== null) {
        setDelta(now - start)
      }
      setStart(null)
    },
  }
}

function SpindleDirection({ raw, apply, canConfigure }: CheckProps) {
  const counter = useCountDelta()
  const inverted = raw?.invertEncoder === "true"
  const suggested = counter.delta === null || counter.delta === 0 ? null : counter.delta < 0 ? !inverted : inverted
  return (
    <Panel title="Spindle direction">
      <p className="mb-3 text-sm text-muted-foreground">Press Start, turn the chuck forward by hand, then press Done.</p>
      <div className="mb-3 flex flex-wrap items-center gap-2">
        <Button onClick={counter.begin}>Start</Button>
        <Button disabled={!counter.started} onClick={counter.end}>
          Done
        </Button>
        {counter.delta !== null && <span className="text-sm">Counts: {counter.delta}</span>}
      </div>
      <Suggestion
        current={inverted ? "inverted" : "normal"}
        suggested={suggested === null ? null : suggested ? "inverted" : "normal"}
        disabled={!canConfigure}
        onApply={() => apply("invertEncoder", String(suggested))}
      />
    </Panel>
  )
}

function EncoderPpr({ raw, apply, canConfigure }: CheckProps) {
  const counter = useCountDelta()
  const [turns, setTurns] = useState(10)
  const suggested = counter.delta && turns > 0 ? String(Math.round(Math.abs(counter.delta) / (2 * turns))) : null
  return (
    <Panel title="Encoder PPR">
      <p className="mb-3 text-sm text-muted-foreground">
        Mark the chuck. Press Start, turn the chuck forward by exactly the number of turns below, then press Done.
      </p>
      <div className="mb-3 flex flex-wrap items-end gap-2">
        <Field label="Turns">
          <TextInput type="number" min={1} step={1} value={turns} onChange={(e) => setTurns(Number(e.target.value))} />
        </Field>
        <Button onClick={counter.begin}>Start</Button>
        <Button disabled={!counter.started} onClick={counter.end}>
          Done
        </Button>
        {counter.delta !== null && <span className="text-sm">Counts: {counter.delta}</span>}
      </div>
      <Suggestion
        current={raw?.encoderPpr ?? "?"}
        suggested={suggested}
        disabled={!canConfigure}
        onApply={() => suggested && apply("encoderPpr", suggested)}
      />
    </Panel>
  )
}

const PLUS_LABEL: Record<Axis, string> = { Z: "toward the tailstock", X: "away from the centerline" }
const MINUS_LABEL: Record<Axis, string> = { Z: "toward the chuck", X: "toward the centerline" }

async function move(axis: Axis, du: number, duPerMin?: number): Promise<CmdOutcome> {
  const motion = duPerMin ? `G1 F${duPerMin / DU_PER_MM}` : "G0"
  return sendCommand(`@G21 G91 ${motion} ${axis}${du / DU_PER_MM}`)
}

function AxisDirectionRow({ axis, raw, apply, canConfigure }: CheckProps & { axis: Axis }) {
  const toast = useToast()
  const [moved, setMoved] = useState(false)
  const [correct, setCorrect] = useState<boolean | null>(null)
  const key = `invertDirection${axis}`
  const inverted = raw?.[key] === "true"
  const suggested = correct === null ? null : correct ? inverted : !inverted

  async function go() {
    setCorrect(null)
    const outcome = await move(axis, DIRECTION_MOVE_DU)
    if (outcome.status === "denied") {
      toast.push(outcome.msg, "bad")
      return
    }
    setMoved(true)
  }

  return (
    <div className="flex flex-col gap-2">
      <div className="flex flex-wrap items-center gap-2">
        <Button disabled={!canConfigure} onClick={go}>
          Move {axis}+ {mm(DIRECTION_MOVE_DU, 0)}
        </Button>
        {moved && (
          <>
            <span className="text-sm text-muted-foreground">It moved:</span>
            <Button variant={correct === true ? "primary" : "secondary"} onClick={() => setCorrect(true)}>
              {PLUS_LABEL[axis]}
            </Button>
            <Button variant={correct === false ? "primary" : "secondary"} onClick={() => setCorrect(false)}>
              {MINUS_LABEL[axis]}
            </Button>
          </>
        )}
      </div>
      <Suggestion
        current={inverted ? "inverted" : "normal"}
        suggested={suggested === null ? null : suggested ? "inverted" : "normal"}
        disabled={!canConfigure}
        onApply={() => apply(key, String(suggested))}
      />
    </div>
  )
}

function AxisDirection(props: CheckProps) {
  return (
    <Panel title="Axis direction">
      <p className="mb-3 text-sm text-muted-foreground">Move each axis, then select which way it moved.</p>
      <div className="flex flex-col gap-4">
        <AxisDirectionRow axis="Z" {...props} />
        <AxisDirectionRow axis="X" {...props} />
      </div>
    </Panel>
  )
}

function StepsPerMmRow({ axis, raw, apply, canConfigure }: CheckProps & { axis: Axis }) {
  const toast = useToast()
  const [commandedMm, setCommandedMm] = useState(10)
  const [measuredMm, setMeasuredMm] = useState<number | null>(null)
  const key = `screwPitchDu${axis}`
  const pitch = Number(raw?.[key] ?? 0)
  const suggested =
    measuredMm && commandedMm > 0 && pitch > 0 ? String(Math.round((pitch * measuredMm) / commandedMm)) : null

  async function run(du: number) {
    const outcome = await move(axis, -du, STEPS_PER_MM_DU_PER_MIN)
    if (outcome.status === "denied") {
      toast.push(outcome.msg, "bad")
    }
  }

  return (
    <div className="flex flex-col gap-2">
      <div className="flex flex-wrap items-end gap-2">
        <Button disabled={!canConfigure} onClick={() => run(PRELOAD_MOVE_DU)}>
          {axis}: Preload {mm(PRELOAD_MOVE_DU, 0)}
        </Button>
        <Field label="Commanded (mm)">
          <TextInput
            type="number"
            min={0.1}
            step={0.1}
            value={commandedMm}
            onChange={(e) => setCommandedMm(Number(e.target.value))}
          />
        </Field>
        <Button disabled={!canConfigure || commandedMm <= 0} onClick={() => run(Math.round(commandedMm * DU_PER_MM))}>
          Move {axis}-
        </Button>
        <Field label="Measured (mm)">
          <TextInput
            type="number"
            step={0.001}
            value={measuredMm ?? ""}
            onChange={(e) => setMeasuredMm(e.target.value === "" ? null : Number(e.target.value))}
          />
        </Field>
      </div>
      <Suggestion
        current={mm(pitch)}
        suggested={suggested === null ? null : mm(Number(suggested))}
        disabled={!canConfigure}
        onApply={() => suggested && apply(key, suggested)}
      />
    </div>
  )
}

function StepsPerMm(props: CheckProps) {
  return (
    <Panel title="Steps/mm">
      <p className="mb-3 text-sm text-muted-foreground">
        Put a dial indicator against the axis. Press Preload to take up the backlash, then zero the indicator. Press Move,
        then enter the indicator reading. The check corrects the lead screw pitch.
      </p>
      <div className="flex flex-col gap-4">
        <StepsPerMmRow axis="Z" {...props} />
        <StepsPerMmRow axis="X" {...props} />
      </div>
    </Panel>
  )
}

function BacklashRow({ axis, raw, apply, canConfigure }: CheckProps & { axis: Axis }) {
  const { state } = useDeviceState()
  const toast = useToast()
  const [results, setResults] = useState<Partial<Record<Side, number>>>({})
  const [active, setActive] = useState<Side | null>(null)
  const seenRunning = useRef(false)
  const key = `backlashDu${axis}`

  useEffect(() => {
    if (!active || !state) {
      return
    }
    if (state.running) {
      seenRunning.current = true
      return
    }
    if (!seenRunning.current) {
      return
    }
    const side = active
    setActive(null)
    if (state.error) {
      toast.push(`Backlash ${axis}${side}: ${state.error}`, "bad")
      return
    }
    sendCommand("cal status").then((outcome) => {
      const du = replyValue(outcome, "backlash")
      if (du === null || du < 0) {
        toast.push(denial(outcome), "bad")
      } else {
        setResults((r) => ({ ...r, [side]: du }))
      }
    })
    // eslint-disable-next-line react-hooks/exhaustive-deps
  }, [state, active])

  async function measure(side: Side) {
    seenRunning.current = false
    const outcome = await sendCommand(`cal backlash ${axis.toLowerCase()} ${side} ${BACKLASH_RUNS}`)
    if (outcome.status === "ok") {
      setActive(side)
    } else {
      toast.push(denial(outcome), "bad")
    }
  }

  const values = Object.values(results)
  const average = values.length ? Math.round(values.reduce((a, b) => a + b, 0) / values.length) : null
  const minus = results["-"]
  const plus = results["+"]
  const apart =
    minus !== undefined && plus !== undefined && Math.abs(minus - plus) > BACKLASH_SIDE_TOLERANCE * Math.max(minus, plus)
  const job = state?.job

  return (
    <div className="flex flex-col gap-2">
      <div className="flex flex-wrap items-center gap-2">
        {(["-", "+"] as Side[]).map((side) => (
          <Button key={side} disabled={!canConfigure || active !== null} onClick={() => measure(side)}>
            Probe {axis}
            {side}
          </Button>
        ))}
        {active && (
          <>
            <span className="text-sm text-muted-foreground">
              {job?.phase} {job && job.passes > 0 && `${job.pass}/${job.passes}`}
            </span>
            <Button variant="danger" onClick={() => sendCommand("cal stop")}>
              Stop
            </Button>
          </>
        )}
        {minus !== undefined && <span className="text-sm">{axis}-: {mm(minus)}</span>}
        {plus !== undefined && <span className="text-sm">{axis}+: {mm(plus)}</span>}
        {apart && <Badge tone="warn">sides differ: check the probe and the rigidity</Badge>}
      </div>
      <Suggestion
        current={mm(Number(raw?.[key] ?? 0))}
        suggested={average === null ? null : mm(average)}
        disabled={!canConfigure}
        onApply={() => average !== null && apply(key, String(average))}
      />
    </div>
  )
}

function Backlash(props: CheckProps) {
  return (
    <Panel title="Backlash">
      <p className="mb-3 text-sm text-muted-foreground">
        Use a contact plate, not a switch. Set it against the workpiece and jog the tool near it. Probe from one side or
        both. The probe settings set the feeds, the back-off and the probe distance.
      </p>
      <div className="flex flex-col gap-4">
        <BacklashRow axis="Z" {...props} />
        <BacklashRow axis="X" {...props} />
      </div>
    </Panel>
  )
}
