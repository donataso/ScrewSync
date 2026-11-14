"use client"

import { useEffect, useState } from "react"
import { sendCommand } from "../api"
import { useFooterBar } from "../footer-bar"
import { CONFIGURE_LOCK_MESSAGE, useCanConfigure } from "../state-provider"
import {
  MACHINE_CONFIG_SECTIONS,
  parseMachineConfig,
  toDisplayValue,
  toRawValue,
  type MachineConfigField,
  type MachineConfigPair,
} from "../machine-config"
import { Button, Field, InfoIcon, LockNotice, Panel, TextInput, Toggle } from "../ui/primitives"
import { useToast } from "../ui/toast"

export function SettingsScreen() {
  const [raw, setRaw] = useState<Record<string, string> | null>(null)
  const [dirty, setDirty] = useState<Record<string, string>>({})
  const [loading, setLoading] = useState(false)
  const [busy, setBusy] = useState(false)
  const canConfigure = useCanConfigure()
  const toast = useToast()

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
      <div className="mx-auto flex max-w-5xl gap-2 px-4 py-3">
        <Button variant="primary" disabled={busy || loading || !canConfigure || Object.keys(dirty).length === 0} onClick={save}>
          Save
        </Button>
        <Button variant="danger" disabled={busy || loading || !canConfigure} onClick={reset}>
          Reset to defaults
        </Button>
      </div>
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
        MACHINE_CONFIG_SECTIONS.map((section) => (
          <Panel key={section.title} title={section.title}>
            <div className="flex flex-col gap-4">
              {section.fields.length > 0 && (
                <div className="grid gap-3 sm:grid-cols-2 md:grid-cols-4">
                  {section.fields.map((field) => (
                    <Field key={field.key} label={fieldLabel(field)} tooltip={field.help}>
                      <FieldInput field={field} value={valueOf(field)} onChange={(v) => setField(field, v)} />
                    </Field>
                  ))}
                </div>
              )}
              {section.pairs && (
                <div className="grid gap-3 md:grid-cols-2">
                  {section.pairs.map((p) => (
                    <PairRow
                      key={p.z.key}
                      pair={p}
                      zValue={valueOf(p.z)}
                      xValue={valueOf(p.x)}
                      onChange={(field, v) => setField(field, v)}
                    />
                  ))}
                </div>
              )}
            </div>
          </Panel>
        ))
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

function PairRow({
  pair,
  zValue,
  xValue,
  onChange,
}: {
  pair: MachineConfigPair
  zValue: Value
  xValue: Value
  onChange: (field: MachineConfigField, v: Value) => void
}) {
  const showCopy = pair.copy && zValue !== xValue
  const copyButton = (label: string, title: string, onClick: () => void) => (
    <Button variant="ghost" className={`px-2 ${showCopy ? "" : "invisible"}`} title={title} onClick={onClick}>
      {label}
    </Button>
  )
  return (
    <div className="flex flex-col gap-1 text-sm">
      <span className="text-muted-foreground">{fieldLabel(pair.z)}</span>
      <div className="grid grid-cols-[auto_1fr_auto_auto_1fr] items-center gap-1.5">
        <AxisTag field={pair.z} axis="Z" />
        <FieldInput field={pair.z} value={zValue} onChange={(v) => onChange(pair.z, v)} />
        {copyButton("→", "Copy Z to X", () => onChange(pair.x, zValue))}
        {copyButton("←", "Copy X to Z", () => onChange(pair.z, xValue))}
        <span className="flex items-center gap-1.5">
          <AxisTag field={pair.x} axis="X" />
          <FieldInput field={pair.x} value={xValue} onChange={(v) => onChange(pair.x, v)} />
        </span>
      </div>
    </div>
  )
}

function AxisTag({ field, axis }: { field: MachineConfigField; axis: string }) {
  return (
    <span className="flex items-center gap-1 text-xs font-medium text-muted-foreground">
      {axis}
      <InfoIcon tooltip={field.help} />
    </span>
  )
}
