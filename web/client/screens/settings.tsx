"use client"

import { useEffect, useState } from "react"
import { sendCommand } from "../api"
import { SaveBar, useFooterBar } from "../footer-bar"
import { CONFIGURE_LOCK_MESSAGE, useCanConfigure, useDeviceState } from "../state-provider"
import {
  configFields,
  parseMachineConfig,
  sectionsInUnits,
  toDisplayValue,
  toRawValue,
  type MachineConfigField,
  type MachineConfigPair,
} from "../machine-config"
import { InfoIcon, LockNotice, Panel, TextInput, Toggle } from "../ui/primitives"
import { useToast } from "../ui/toast"

export function SettingsScreen() {
  const [raw, setRaw] = useState<Record<string, string> | null>(null)
  const [dirty, setDirty] = useState<Record<string, string>>({})
  const [loading, setLoading] = useState(false)
  const [busy, setBusy] = useState(false)
  const canConfigure = useCanConfigure()
  const toast = useToast()
  const sections = sectionsInUnits(useDeviceState().state?.units ?? "mm")

  async function load() {
    setLoading(true)
    try {
      const outcome = await sendCommand("settings get")
      if (outcome.status === "ok") {
        setRaw(parseMachineConfig(outcome.data))
        setDirty({})
      } else {
        toast.push(outcome.status === "denied" ? `Denied: ${outcome.msg}` : "No reply from device.", "bad")
      }
    } finally {
      setLoading(false)
    }
  }

  useEffect(() => {
    load()
    // eslint-disable-next-line react-hooks/exhaustive-deps
  }, [])

  useFooterBar(
    raw && (
      <SaveBar
        dirty={configFields(sections).some((f) => f.key in dirty && valueOf(f) !== toDisplayValue(f, raw[f.key]))}
        saveDisabled={busy || loading || !canConfigure}
        discardDisabled={busy || loading}
        resetDisabled={busy || loading || !canConfigure}
        onSave={save}
        onDiscard={() => setDirty({})}
        onReset={reset}
      />
    ),
  )

  function valueOf(field: MachineConfigField) {
    return toDisplayValue(field, dirty[field.key] ?? raw?.[field.key])
  }

  function setField(field: MachineConfigField, display: number | boolean) {
    setDirty((d) => ({ ...d, [field.key]: toRawValue(field, display) }))
  }

  async function save() {
    if (!raw) {
return
}
    setBusy(true)
    try {
      const denials: string[] = []
      let changed = false
      for (const [key, value] of Object.entries(dirty)) {
        if (value === raw[key]) {
continue
}
        changed = true
        const r = await sendCommand(`settings set ${key} ${value}`)
        if (r.status === "ok") {
raw[key] = value
} else {
denials.push(r.status === "denied" ? r.msg : `${key}: no reply`)
}
      }
      if (!changed) {
toast.push("Nothing changed.", "info")
} else if (denials.length) {
toast.push(`Some changes were denied: ${denials.join("; ")}`, "bad")
} else {
toast.push("Applied and saved to flash.", "good")
}
      setDirty({})
      setRaw({ ...raw })
    } finally {
      setBusy(false)
    }
  }

  async function reset() {
    if (!window.confirm("Replace all settings with the defaults?")) {
      return
    }
    setBusy(true)
    try {
      const r = await sendCommand("settings reset")
      if (r.status === "ok") {
        toast.push("Reset to defaults.", "good")
        await load()
      } else {
        toast.push(r.status === "denied" ? `Denied: ${r.msg}` : "No reply from device.", "bad")
      }
    } finally {
      setBusy(false)
    }
  }

  return (
    <div className="flex flex-col gap-4">
      <h1 className="text-lg font-semibold">Settings</h1>
      {raw && !canConfigure && <LockNotice>{CONFIGURE_LOCK_MESSAGE}</LockNotice>}
      {!raw ? (
        <Panel title="Machine configuration">
          <p className="text-sm text-muted-foreground">{loading ? "Loading..." : "Not loaded."}</p>
        </Panel>
      ) : (
        <fieldset disabled={!canConfigure} className="contents">
          {sections.map((section) => (
          <Panel key={section.title} title={section.title}>
            <div className="flex flex-col gap-4">
              {section.fields.length > 0 && (
                <div className={GRID}>
                  {section.fieldsTitle && <div className={`col-span-2 md:col-span-4 ${SUBTITLE}`}>{section.fieldsTitle}</div>}
                  {section.fields.map((field) => (
                    <SettingCell key={field.key} field={field} value={valueOf(field)} onChange={(v) => setField(field, v)} />
                  ))}
                </div>
              )}
              {section.fields.length > 0 && section.pairs && <hr className="my-2 border-border" />}
              {section.common && (
                <div className={GRID}>
                  {section.commonTitle && <div className={`col-span-2 md:col-span-4 ${SUBTITLE}`}>{section.commonTitle}</div>}
                  {section.common.map((field) => (
                    <SettingCell key={field.key} field={field} value={valueOf(field)} onChange={(v) => setField(field, v)} />
                  ))}
                </div>
              )}
              {section.pairs && <AxisGrid pairs={section.pairs} title={section.pairsTitle} valueOf={valueOf} onChange={setField} />}
            </div>
          </Panel>
          ))}
        </fieldset>
      )}
    </div>
  )
}

type Value = number | boolean

function fieldLabel(field: MachineConfigField) {
  return field.unit ? `${field.label} (${field.unit})` : field.label
}

function FieldInput({ field, value, onChange }: { field: MachineConfigField; value: Value; onChange: (v: Value) => void }) {
  if (field.type === "checkbox") {
    return (
      <span className="flex h-[34px] items-center">
        <Toggle checked={value as boolean} onChange={onChange} label={field.label} />
      </span>
    )
  }
  return (
    <TextInput
      type="number"
      className="w-full min-w-0"
      min={field.min}
      max={field.max}
      step={field.step}
      value={value as number}
      onChange={(e) => onChange(Number(e.target.value))}
    />
  )
}

const GRID = "grid grid-cols-2 gap-x-6 gap-y-3 md:grid-cols-4"
const SUBTITLE = "text-xs font-semibold uppercase tracking-wide text-muted-foreground"

function AxisGrid({
  pairs,
  title,
  valueOf,
  onChange,
}: {
  pairs: MachineConfigPair[]
  title?: string
  valueOf: (field: MachineConfigField) => Value
  onChange: (field: MachineConfigField, v: Value) => void
}) {
  const rows: MachineConfigPair[][] = []
  for (let i = 0; i < pairs.length; i += 2) {
    rows.push(pairs.slice(i, i + 2))
  }
  const cell = (p: MachineConfigPair | undefined, axis: "Z" | "X", key: string) => {
    if (!p) {
      return <div key={key} />
    }
    const [field, other] = axis === "Z" ? [p.z, p.x] : [p.x, p.z]
    const value = valueOf(field)
    const otherValue = valueOf(other)
    return (
      <SettingCell
        key={field.key}
        field={field}
        axis={axis}
        value={value}
        onChange={(v) => onChange(field, v)}
        copy={
          p.copy && value !== otherValue
            ? { label: `Copy to ${axis === "Z" ? "X" : "Z"}`, onClick: () => onChange(other, value) }
            : undefined
        }
      />
    )
  }
  return (
    <div className="relative">
      <div className="absolute inset-y-0 left-1/2 hidden border-l border-border md:block" />
      <div className={GRID}>
        <div className={`col-span-2 hidden md:block ${SUBTITLE}`}>{title ? `Z axis ${title}` : "Z axis"}</div>
        <div className={`col-span-2 hidden md:block ${SUBTITLE}`}>{title ? `X axis ${title}` : "X axis"}</div>
        {rows.flatMap((r, i) => [
          cell(r[0], "Z", `z0-${i}`),
          cell(r[1], "Z", `z1-${i}`),
          cell(r[0], "X", `x0-${i}`),
          cell(r[1], "X", `x1-${i}`),
        ])}
      </div>
    </div>
  )
}

function SettingCell({
  field,
  axis,
  value,
  onChange,
  copy,
}: {
  field: MachineConfigField
  axis?: string
  value: Value
  onChange: (v: Value) => void
  copy?: { label: string; onClick: () => void }
}) {
  return (
    <div className="flex min-w-0 flex-col gap-1 text-sm">
      <div className="flex items-center gap-1 text-muted-foreground">
        {axis && <span className="font-semibold text-foreground md:hidden">{axis}</span>}
        <span className="truncate">{fieldLabel(field)}</span>
        <InfoIcon tooltip={field.help} />
        {copy && (
          <button
            type="button"
            className="ml-auto shrink-0 cursor-pointer rounded-full border border-sky-500/40 bg-sky-500/10 px-2 py-px text-xs font-medium text-sky-700 hover:bg-sky-500/20 dark:text-sky-300 disabled:cursor-not-allowed disabled:opacity-40"
            onClick={copy.onClick}
          >
            {copy.label}
          </button>
        )}
      </div>
      <FieldInput field={field} value={value} onChange={onChange} />
    </div>
  )
}
