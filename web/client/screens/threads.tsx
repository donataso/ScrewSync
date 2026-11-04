"use client"

import { useEffect, useRef, useState } from "react"
import { fetchThreads, saveThreads, sendCommand } from "../api"
import { useFooterBar } from "../footer-bar"
import { CONFIGURE_LOCK_MESSAGE, useCanConfigure, useDeviceState } from "../state-provider"
import {
  DESIGNATION_MAX_LEN,
  MAX_GROUP_ENTRIES,
  MAX_GROUPS,
  NAME_MAX_LEN,
  formatLength,
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
  tableValid,
  taperError,
  toJson,
  type ThreadEntry,
  type ThreadGroup,
  type Units,
} from "../threads"
import { Badge, Button, Field, LockNotice, Panel, Select, TextInput } from "../ui/primitives"
import { TaperInput } from "../ui/taper-input"
import { ThreadSketch } from "../ui/thread-sketch"
import { useToast } from "../ui/toast"

const NEW_GROUP_DEPTH_EXTERNAL = "0.61343"
const NEW_GROUP_DEPTH_INTERNAL = "0.54127"
const NEW_GROUP_FLANK_ANGLE = "60"
const ROUTE = "/threads"

function hashGroup(): string {
  const path = window.location.hash.slice(1).replace(/^\/?/, "/")
  return path.startsWith(ROUTE + "/") ? decodeURIComponent(path.slice(ROUTE.length + 1)) : ""
}

export function ThreadsScreen() {
  const [groups, setGroups] = useState<ThreadGroup[] | null>(null)
  const [saved, setSaved] = useState("")
  const [sel, setSel] = useState(0)
  const [editing, setEditing] = useState<number | "new" | null>(null)
  const [loading, setLoading] = useState(false)
  const [busy, setBusy] = useState(false)
  const [loadCount, setLoadCount] = useState(0)
  const units: Units = useDeviceState().state?.units ?? "mm"
  const canConfigure = useCanConfigure()
  const toast = useToast()

  async function load() {
    setLoading(true)
    try {
      const table = fromJson(await fetchThreads())
      setGroups(table)
      setSaved(JSON.stringify(toJson(table)))
      const name = groups?.[sel]?.name ?? hashGroup()
      setSel(Math.max(table.findIndex((g) => g.name === name), 0))
      setEditing(null)
      setLoadCount((n) => n + 1)
    } catch (e) {
      toast.push(`Could not load threads: ${(e as Error).message}`, "bad")
    } finally {
      setLoading(false)
    }
  }

  useEffect(() => {
    load()
  }, [])

  const selName = groups?.[sel]?.name
  useEffect(() => {
    if (selName !== undefined) window.history.replaceState(null, "", `#${ROUTE}/${encodeURIComponent(selName)}`)
  }, [selName])

  const dirty = groups !== null && JSON.stringify(toJson(groups)) !== saved
  const valid = groups !== null && tableValid(groups)

  async function save() {
    if (!groups) return
    setBusy(true)
    try {
      const json = toJson(groups)
      const r = await saveThreads(json)
      if (r.ok) {
        setSaved(JSON.stringify(json))
        toast.push("Thread table saved.", "good")
      } else {
        toast.push(`Not saved: ${r.message}`, "bad")
      }
    } finally {
      setBusy(false)
    }
  }

  async function reset() {
    if (!window.confirm("Replace the whole thread table with the built-in defaults?")) return
    setBusy(true)
    try {
      const r = await sendCommand("threads reset")
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

  useFooterBar(
    groups && (
      <div className="mx-auto flex max-w-5xl flex-wrap gap-2 px-4 py-3">
        <Button variant="danger" disabled={busy || loading || !canConfigure} onClick={reset}>
          Reset to defaults
        </Button>
        <div className="ml-auto flex gap-2">
          <Button disabled={busy || loading || !dirty} onClick={load}>
            Discard
          </Button>
          <Button variant="primary" disabled={busy || loading || !canConfigure || !dirty || !valid} onClick={save}>
            Save
          </Button>
        </div>
      </div>
    ),
  )

  if (!groups) {
    return (
      <div className="flex flex-col gap-4">
        <h1 className="text-lg font-semibold">Threads</h1>
        <Panel>
          <p className="text-sm text-muted-foreground">{loading ? "Loading..." : "Not loaded."}</p>
        </Panel>
      </div>
    )
  }

  const group = groups[sel]

  function updateGroup(patch: Partial<ThreadGroup>) {
    setGroups((gs) => gs && gs.map((g, i) => (i === sel ? { ...g, ...patch } : g)))
  }

  function setEntries(entries: ThreadEntry[]) {
    updateGroup({ entries })
  }

  function addGroup() {
    if (!groups) return
    const g: ThreadGroup = {
      name: newGroupName(groups),
      depthExternal: NEW_GROUP_DEPTH_EXTERNAL,
      depthInternal: NEW_GROUP_DEPTH_INTERNAL,
      taper: 0,
      flankAngle: NEW_GROUP_FLANK_ANGLE,
      entries: [],
    }
    setGroups([...groups, g])
    setSel(groups.length)
  }

  function deleteGroup() {
    if (!groups) return
    const threads = group.entries.length ? ` and its ${group.entries.length} threads` : ""
    if (!window.confirm(`Delete group "${group.name}"${threads}?`)) return
    setGroups(groups.filter((_, i) => i !== sel))
    setSel(Math.max(sel - 1, 0))
  }

  function finishEdit(entry: ThreadEntry) {
    if (editing === "new") setEntries([...group.entries, entry])
    else if (editing !== null) setEntries(group.entries.map((e, i) => (i === editing ? entry : e)))
    setEditing(null)
  }

  function deleteEntry() {
    if (typeof editing === "number") setEntries(group.entries.filter((_, i) => i !== editing))
    setEditing(null)
  }

  const errors = groupErrors(groups, sel)
  const full = group.entries.length >= MAX_GROUP_ENTRIES

  return (
    <div className="flex flex-col gap-4">
      {!canConfigure && <LockNotice>{CONFIGURE_LOCK_MESSAGE}</LockNotice>}
      <div className="flex flex-wrap items-center gap-2">
        <h1 className="mr-2 text-lg font-semibold">Threads</h1>
        <Select value={sel} onChange={(e) => setSel(Number(e.target.value))} aria-label="Group">
          {groups.map((g, i) => (
            <option key={i} value={i}>
              {g.name}
            </option>
          ))}
        </Select>
        <Button disabled={groups.length >= MAX_GROUPS} onClick={addGroup}>
          + Group
        </Button>
        <div className="ml-auto flex items-center gap-2">
          <span className="text-sm text-muted-foreground tabular-nums">
            {group.entries.length} / {MAX_GROUP_ENTRIES}
          </span>
          <Button variant="primary" disabled={full} onClick={() => setEditing("new")}>
            + Thread
          </Button>
        </div>
      </div>

      <Panel
        title="Group"
        action={
          <Button variant="danger" disabled={groups.length <= 1} onClick={deleteGroup}>
            Delete group
          </Button>
        }
      >
        <div className="flex flex-col gap-3">
          <div className="grid gap-3 sm:grid-cols-4">
            <Field label="Name" error={errors.name}>
              <TextInput value={group.name} maxLength={NAME_MAX_LEN} onChange={(e) => updateGroup({ name: e.target.value })} />
            </Field>
            <Field label="Depth external (× pitch)" error={errors.depthExternal}>
              <TextInput inputMode="decimal" value={group.depthExternal} onChange={(e) => updateGroup({ depthExternal: e.target.value })} />
            </Field>
            <Field label="Depth internal (× pitch)" error={errors.depthInternal}>
              <TextInput inputMode="decimal" value={group.depthInternal} onChange={(e) => updateGroup({ depthInternal: e.target.value })} />
            </Field>
            <Field label="Flank angle (°)" error={errors.flankAngle}>
              <TextInput inputMode="decimal" value={group.flankAngle} onChange={(e) => updateGroup({ flankAngle: e.target.value })} />
            </Field>
          </div>
          <TaperInput key={`${loadCount}-${sel}`} value={group.taper} onChange={(taper) => updateGroup({ taper: taper ?? 0 })} />
        </div>
      </Panel>

      <ThreadList entries={group.entries} units={units} onReorder={setEntries} onOpen={setEditing} />

      {editing !== null && (
        <ThreadEditor
          group={group}
          entry={editing === "new" ? undefined : group.entries[editing]}
          taken={group.entries.filter((_, i) => i !== editing).map((e) => e.designation)}
          units={units}
          onDone={finishEdit}
          onDelete={editing === "new" ? undefined : deleteEntry}
          onCancel={() => setEditing(null)}
        />
      )}
    </div>
  )
}

function ThreadList({
  entries,
  units,
  onReorder,
  onOpen,
}: {
  entries: ThreadEntry[]
  units: Units
  onReorder: (entries: ThreadEntry[]) => void
  onOpen: (index: number) => void
}) {
  const listRef = useRef<HTMLDivElement>(null)
  const [dragIndex, setDragIndex] = useState<number | null>(null)

  function onPointerMove(e: React.PointerEvent) {
    if (dragIndex === null || !listRef.current) return
    const rows = Array.from(listRef.current.children)
    let target = rows.length - 1
    for (let i = 0; i < rows.length; i++) {
      const r = rows[i].getBoundingClientRect()
      if (e.clientY < r.top + r.height / 2) {
        target = i
        break
      }
    }
    if (target === dragIndex) return
    const next = [...entries]
    const [moved] = next.splice(dragIndex, 1)
    next.splice(target, 0, moved)
    onReorder(next)
    setDragIndex(target)
  }

  if (!entries.length) {
    return (
      <Panel>
        <p className="text-sm text-muted-foreground">No threads in this group.</p>
      </Panel>
    )
  }

  return (
    <div ref={listRef} className="flex flex-col overflow-hidden rounded-lg border border-border bg-card">
      {entries.map((e, i) => (
        <div
          key={e.designation}
          className={`flex items-center border-b border-border last:border-b-0 ${dragIndex === i ? "bg-muted" : ""}`}
        >
          <span
            className="flex cursor-grab touch-none items-center self-stretch px-3 text-muted-foreground active:cursor-grabbing"
            aria-label={`Drag ${e.designation}`}
            onPointerDown={(ev) => {
              ev.preventDefault()
              ev.currentTarget.setPointerCapture(ev.pointerId)
              setDragIndex(i)
            }}
            onPointerMove={onPointerMove}
            onPointerUp={() => setDragIndex(null)}
            onPointerCancel={() => setDragIndex(null)}
          >
            <svg viewBox="0 0 10 16" className="h-4 w-2.5 fill-current" aria-hidden>
              {[2, 8, 14].flatMap((y) => [2, 8].map((x) => <circle key={`${x}-${y}`} cx={x} cy={y} r={1.5} />))}
            </svg>
          </span>
          <button
            type="button"
            onClick={() => onOpen(i)}
            className="grid flex-1 grid-cols-[1fr_auto_auto] items-center gap-4 py-2.5 pr-4 text-left text-sm hover:bg-muted/60"
          >
            <span className="flex items-center gap-2 font-medium">
              {e.designation}
              {hasOverride(e) && <Badge tone="info">own</Badge>}
            </span>
            <span className="w-24 text-right tabular-nums text-muted-foreground">
              ⌀ {formatLength(e.majorDu, units)} {lengthUnit(units)}
            </span>
            <span className="w-24 text-right tabular-nums text-muted-foreground">
              {units === "inch" ? `${formatTpi(e.pitchDu)} TPI` : `${formatLength(e.pitchDu, units)} mm`}
            </span>
          </button>
        </div>
      ))}
    </div>
  )
}

function formatPitch(pitchDu: number, units: Units): string {
  return units === "inch" ? formatTpi(pitchDu) : formatLength(pitchDu, units)
}

function ThreadEditor({
  group,
  entry,
  taken,
  units,
  onDone,
  onDelete,
  onCancel,
}: {
  group: ThreadGroup
  entry?: ThreadEntry
  taken: string[]
  units: Units
  onDone: (entry: ThreadEntry) => void
  onDelete?: () => void
  onCancel: () => void
}) {
  const [edUnits, setEdUnits] = useState<Units>(units)
  const unit = lengthUnit(edUnits)
  const [designation, setDesignation] = useState(entry?.designation ?? "")
  const [major, setMajor] = useState(entry ? formatLength(entry.majorDu, units) : "")
  const [pitch, setPitch] = useState(entry ? formatPitch(entry.pitchDu, units) : "")
  const [depthExt, setDepthExt] = useState(entry?.depthExternalDu !== undefined ? formatLength(entry.depthExternalDu, units) : "")
  const [depthInt, setDepthInt] = useState(entry?.depthInternalDu !== undefined ? formatLength(entry.depthInternalDu, units) : "")
  const [taper, setTaper] = useState<number | undefined>(entry?.taper)

  useEffect(() => {
    const onKey = (e: KeyboardEvent) => e.key === "Escape" && onCancel()
    window.addEventListener("keydown", onKey)
    return () => window.removeEventListener("keydown", onKey)
  }, [onCancel])

  const pitchDu = edUnits === "inch" ? parseTpi(pitch) : parseLength(pitch, edUnits)
  const majorDu = parseLength(major, edUnits)
  const pitchOk = pitchDu > 0

  // Converts every dimension field; unparsable text is left as typed.
  function switchUnits(next: Units) {
    if (next === edUnits) return
    const convert = (text: string, set: (t: string) => void) => {
      const du = parseLength(text, edUnits)
      if (Number.isFinite(du)) set(formatLength(du, next))
    }
    convert(major, setMajor)
    convert(depthExt, setDepthExt)
    convert(depthInt, setDepthInt)
    if (pitchOk) setPitch(formatPitch(pitchDu, next))
    setEdUnits(next)
  }

  function depthError(text: string): string | undefined {
    if (text.trim() === "") return undefined
    const du = parseLength(text, edUnits)
    if (Number.isNaN(du)) return "Not a number"
    if (!(du > 0)) return "Must be above 0"
    if (pitchOk && du >= 2 * pitchDu) return `Must be under ${formatLength(2 * pitchDu, edUnits)} ${unit}`
    return undefined
  }

  const errors = {
    designation: nameError(designation, taken, "Designation", DESIGNATION_MAX_LEN),
    major: Number.isNaN(majorDu) ? "Not a number" : majorDu < 0 ? "Must be 0 or more" : undefined,
    pitch: pitchOk ? undefined : "Must be above 0",
    depthExt: depthError(depthExt),
    depthInt: depthError(depthInt),
    taper: taperError(taper),
  }
  const valid = Object.values(errors).every((e) => !e)

  function done() {
    onDone({
      designation,
      majorDu,
      pitchDu,
      depthExternalDu: depthExt.trim() ? parseLength(depthExt, edUnits) : undefined,
      depthInternalDu: depthInt.trim() ? parseLength(depthInt, edUnits) : undefined,
      taper,
    })
  }

  const depthPlaceholder = (internal: boolean) => formatLength(groupDepthDu(group, pitchOk ? pitchDu : 0, internal), edUnits)
  const ratio = taper ?? group.taper

  return (
    <div className="fixed inset-0 z-50 flex items-stretch justify-center bg-black/50 sm:items-center" onClick={onCancel}>
      <div
        role="dialog"
        aria-modal="true"
        aria-label={entry ? `Edit ${entry.designation}` : "New thread"}
        onClick={(e) => e.stopPropagation()}
        className="flex h-full w-full flex-col overflow-y-auto bg-card sm:h-auto sm:max-h-[90vh] sm:max-w-lg sm:rounded-lg sm:border sm:border-border"
      >
        <header className="flex items-center justify-between border-b border-border px-4 py-2.5">
          <h2 className="text-sm font-semibold">
            {entry ? entry.designation : "New thread"} <span className="font-normal text-muted-foreground">in {group.name}</span>
          </h2>
        </header>
        <div className="flex flex-col gap-3 p-4">
          <Field label="Designation" error={errors.designation}>
            <TextInput autoFocus value={designation} maxLength={DESIGNATION_MAX_LEN} onChange={(e) => setDesignation(e.target.value)} />
          </Field>
          <div className="grid grid-cols-2 gap-3">
            <Field label={`Major diameter (${unit})`} error={errors.major}>
              <TextInput inputMode="decimal" value={major} onChange={(e) => setMajor(e.target.value)} />
            </Field>
            <Field label="Pitch" error={errors.pitch}>
              <div className="flex gap-1">
                <TextInput inputMode="decimal" className="min-w-0 flex-1" value={pitch} onChange={(e) => setPitch(e.target.value)} />
                <Select value={edUnits} onChange={(e) => switchUnits(e.target.value as Units)} aria-label="Units">
                  <option value="mm">mm</option>
                  <option value="inch">TPI</option>
                </Select>
              </div>
            </Field>
          </div>
          <div className="grid grid-cols-2 gap-3">
            <Field label={`Depth external (${unit})`} error={errors.depthExt} hint="Empty = group's">
              <TextInput inputMode="decimal" value={depthExt} placeholder={depthPlaceholder(false)} onChange={(e) => setDepthExt(e.target.value)} />
            </Field>
            <Field label={`Depth internal (${unit})`} error={errors.depthInt} hint="Empty = group's">
              <TextInput inputMode="decimal" value={depthInt} placeholder={depthPlaceholder(true)} onChange={(e) => setDepthInt(e.target.value)} />
            </Field>
          </div>
          <TaperInput value={taper} inherited={group.taper} onChange={setTaper} />
          <div className="grid grid-cols-2 gap-3 pt-1">
            <ThreadSketch ratio={ratio} internal={false} />
            <ThreadSketch ratio={ratio} internal />
          </div>
        </div>
        <footer className="mt-auto flex gap-2 border-t border-border px-4 py-3">
          {onDelete && (
            <Button variant="danger" onClick={onDelete}>
              Delete
            </Button>
          )}
          <div className="ml-auto flex gap-2">
            <Button onClick={onCancel}>Cancel</Button>
            <Button variant="primary" disabled={!valid} onClick={done}>
              Done
            </Button>
          </div>
        </footer>
      </div>
    </div>
  )
}
