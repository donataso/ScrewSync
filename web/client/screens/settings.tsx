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
} from "../machine-config"
import { Button, Field, LockNotice, Panel, TextInput, Toggle } from "../ui/primitives"
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

  function setField(field: MachineConfigField, display: number | boolean) {
    setDirty((d) => ({ ...d, [field.key]: toRawValue(field, display) }))
  }

  async function save() {
    if (!raw) return
    setBusy(true)
    try {
      const denials: string[] = []
      let changed = false
      for (const [key, value] of Object.entries(dirty)) {
        if (value === raw[key]) continue
        changed = true
        const r = await sendCommand(`settings set ${key} ${value}`)
        if (r.status === "ok") raw[key] = value
        else denials.push(r.status === "denied" ? r.msg : `${key}: no reply`)
      }
      if (!changed) toast.push("Nothing changed.", "info")
      else if (denials.length) toast.push(`Some changes were denied: ${denials.join("; ")}`, "bad")
      else toast.push("Applied and saved to flash.", "good")
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
            <div className="grid gap-3 md:grid-cols-3">
              {section.fields.map((field) => {
                const value = toDisplayValue(field, dirty[field.key] ?? raw[field.key])
                if (field.type === "checkbox") {
                  return (
                    <Field key={field.key} label={field.label} tooltip={field.help}>
                      <span className="flex h-[34px] items-center">
                        <Toggle checked={value as boolean} onChange={(v) => setField(field, v)} label={field.label} />
                      </span>
                    </Field>
                  )
                }
                return (
                  <Field key={field.key} label={field.unit ? `${field.label} (${field.unit})` : field.label} tooltip={field.help}>
                    <TextInput
                      type="number"
                      min={field.min}
                      max={field.max}
                      step={field.step}
                      value={value as number}
                      onChange={(e) => setField(field, Number(e.target.value))}
                    />
                  </Field>
                )
              })}
            </div>
          </Panel>
        ))
      )}
    </div>
  )
}
