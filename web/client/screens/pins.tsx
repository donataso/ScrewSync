"use client"

import { createContext, useContext, useEffect, useMemo, useState, type KeyboardEvent } from "react"
import { fetchPins, savePins, sendCommand } from "../api"
import { useFooterBar } from "../footer-bar"
import {
  FUNCTIONS,
  CONNECTOR_NAME_MAX,
  STRAPPING_NOTE,
  canStartAt,
  connectorLabel,
  connectorOf,
  defaultAssignment,
  fromPinsJson,
  functionsAt,
  nameIssues,
  pinsOf,
  toPinsJson,
  usage,
  validate,
  type Assignment,
  type Board,
  type BoardPin,
  type Connector,
  type ConnectorNames,
  type EncoderType,
  type FunctionDef,
  type PinMap,
  type Pull,
} from "../pins"
import { Badge, Button, LockNotice, Panel, Select, TextInput, Toggle } from "../ui/primitives"
import { CONFIGURE_LOCK_MESSAGE, useCanConfigure } from "../state-provider"
import { useToast } from "../ui/toast"

const BoardContext = createContext<Board | null>(null)

function useBoard(): Board {
  const board = useContext(BoardContext)
  if (!board) {
    throw new Error("useBoard must be used within BoardContext")
  }
  return board
}

// Literal class names, so Tailwind keeps them.
const COLORS = [
  "bg-sky-500/25 border-sky-500 text-sky-700 dark:text-sky-300",
  "bg-violet-500/25 border-violet-500 text-violet-700 dark:text-violet-300",
  "bg-emerald-500/25 border-emerald-500 text-emerald-700 dark:text-emerald-300",
  "bg-amber-500/25 border-amber-500 text-amber-700 dark:text-amber-300",
  "bg-rose-500/25 border-rose-500 text-rose-700 dark:text-rose-300",
  "bg-teal-500/25 border-teal-500 text-teal-700 dark:text-teal-300",
]
const DOTS = ["bg-sky-500", "bg-violet-500", "bg-emerald-500", "bg-amber-500", "bg-rose-500", "bg-teal-500"]

function colorIndex(fnId: string) {
  return FUNCTIONS.findIndex((f) => f.id === fnId) % COLORS.length
}

// Must match the gap-2, min-w-8, gap-1, p-2 and border classes below.
const GRID_GAP_PX = 8
const PIN_MIN_PX = 32
const PIN_GAP_PX = 4
const CONNECTOR_PAD_PX = 18

interface Focus {
  fn?: string
  connector?: string
  gpio?: number
}

interface Span {
  connector: string
  start: number
  length: number
}

interface Loaded {
  board: Board
  map: PinMap
  names: ConnectorNames
}

export function PinsScreen() {
  const [loaded, setLoaded] = useState<Loaded | null>(null)
  const [error, setError] = useState<string | null>(null)

  useEffect(() => {
    fetchPins()
      .then((doc) => setLoaded(fromPinsJson(doc)))
      .catch((e: Error) => setError(e.message))
  }, [])

  if (!loaded) {
    return (
      <div className="flex flex-col gap-4">
        <h1 className="text-lg font-semibold">Pins</h1>
        <Panel>
          <p className="text-sm text-muted-foreground">{error ? `Could not load the pin map: ${error}` : "Loading..."}</p>
        </Panel>
      </div>
    )
  }
  return (
    <BoardContext.Provider value={loaded.board}>
      <PinsEditor initialMap={loaded.map} initialNames={loaded.names} />
    </BoardContext.Provider>
  )
}

function PinsEditor({ initialMap, initialNames }: { initialMap: PinMap; initialNames: ConnectorNames }) {
  const board = useBoard()
  const [saved, setSaved] = useState<PinMap>(initialMap)
  const [map, setMap] = useState<PinMap>(initialMap)
  const [savedNames, setSavedNames] = useState<ConnectorNames>(initialNames)
  const [names, setNames] = useState<ConnectorNames>(initialNames)
  const [busy, setBusy] = useState(false)
  const canConfigure = useCanConfigure()
  const [renaming, setRenaming] = useState<string | null>(null)
  const [hover, setHover] = useState<Focus | null>(null)
  const [selected, setSelected] = useState<Focus | null>(null)
  const [placing, setPlacing] = useState<string | null>(null)
  const [preview, setPreview] = useState<Span | null>(null)
  const [menu, setMenu] = useState<{ connector: string; start: number } | null>(null)
  const toast = useToast()

  const used = useMemo(() => usage(board, map), [board, map])
  const issues = useMemo(() => [...validate(board, map), ...nameIssues(board, names)], [board, map, names])
  const errors = issues.filter((i) => i.level === "error")
  const dirty = JSON.stringify(map) !== JSON.stringify(saved) || JSON.stringify(names) !== JSON.stringify(savedNames)
  const label = (id: string) => connectorLabel(board, names, id)
  const active = hover ?? selected

  useEffect(() => {
    const onPointerDown = (e: PointerEvent) => {
      if (!(e.target instanceof Element) || !e.target.closest("[data-keep-selection]")) {
        setSelected(null)
        setMenu(null)
      }
    }
    document.addEventListener("pointerdown", onPointerDown)
    return () => document.removeEventListener("pointerdown", onPointerDown)
  }, [])

  useFooterBar(
    <div className="mx-auto flex max-w-5xl flex-wrap items-center gap-2 px-4 py-3">
      <Button variant="primary" disabled={busy || !canConfigure || !dirty || errors.length > 0} onClick={save}>
        Save and reboot
      </Button>
      <Button disabled={busy || !dirty} onClick={discard}>
        Discard changes
      </Button>
      {dirty && <span className="text-sm text-muted-foreground">Changes are not saved.</span>}
    </div>,
  )

  async function save() {
    if (!window.confirm("Save the pin map and reboot the controller?")) {
      return
    }
    setBusy(true)
    try {
      const r = await savePins(toPinsJson(map, names))
      if (!r.ok) {
        toast.push(`Not saved: ${r.message}`, "bad")
        return
      }
      setSaved(map)
      setSavedNames(names)
      toast.push("Saved. Rebooting.", "good")
      await sendCommand("reboot")
    } finally {
      setBusy(false)
    }
  }

  function discard() {
    setMap(saved)
    setNames(savedNames)
    setRenaming(null)
    setPlacing(null)
    setMenu(null)
  }

  function rename(id: string, name: string) {
    const trimmed = name.trim()
    setNames((n) => {
      const next = { ...n }
      if (trimmed === "" || trimmed === connectorOf(board, id)?.label) {
        delete next[id]
      } else {
        next[id] = trimmed
      }
      return next
    })
    setRenaming(null)
  }

  function update(fnId: string, change: (a: Assignment) => Assignment) {
    setMap((m) => (m[fnId] ? { ...m, [fnId]: change(m[fnId]) } : m))
  }

  function place(fnId: string, connectorId: string, start: number) {
    const fn = FUNCTIONS.find((f) => f.id === fnId)
    if (!fn) {
      return
    }
    setMap((m) => {
      const old = m[fnId]
      const a = old ? { ...old, connector: connectorId, start } : defaultAssignment(fn, connectorId, start)
      return { ...m, [fnId]: a }
    })
    setPlacing(null)
    setPreview(null)
    setMenu(null)
    setSelected({ fn: fnId })
  }

  function clear(fnId: string) {
    setMap((m) => {
      const next = { ...m }
      delete next[fnId]
      return next
    })
    if (placing === fnId) {
      setPlacing(null)
    }
  }

  function onPinClick(c: Connector, index: number, pin: BoardPin) {
    if (placing) {
      if (canStartAt(board, map, placing, c.id, index)) {
        place(placing, c.id, index)
      }
      return
    }
    if (!("gpio" in pin) || pin.gpio in board.reserved) {
      setMenu(null)
      setSelected({ connector: c.id })
      return
    }
    const uses = used.get(pin.gpio)
    if (uses?.length) {
      setMenu(null)
      setSelected({ fn: uses[0].fn, gpio: pin.gpio })
      return
    }
    setSelected({ gpio: pin.gpio })
    setMenu({ connector: c.id, start: index })
  }

  function pinHighlighted(pin: BoardPin) {
    if (!active || !("gpio" in pin)) {
      return false
    }
    if (active.gpio === pin.gpio) {
      return true
    }
    return !!active.fn && !!used.get(pin.gpio)?.some((u) => u.fn === active.fn)
  }

  function fnHighlighted(fn: FunctionDef) {
    if (!active) {
      return false
    }
    if (active.fn === fn.id) {
      return true
    }
    const a = map[fn.id]
    if (!a) {
      return false
    }
    if (active.connector && a.connector === active.connector) {
      return true
    }
    return active.gpio !== undefined && pinsOf(board, fn, a).includes(active.gpio)
  }

  const maxPins = Math.max(...board.connectors.map((c) => c.pins.length))
  const connectorMinWidth = maxPins * PIN_MIN_PX + (maxPins - 1) * PIN_GAP_PX + CONNECTOR_PAD_PX
  const rows = Math.max(...board.connectors.map((c) => c.row))
  const cols = Math.max(...board.connectors.map((c) => c.col))
  const placingFn = placing ? FUNCTIONS.find((f) => f.id === placing) : undefined

  return (
    <div className="flex flex-col gap-4">
      <div className="flex flex-wrap items-center justify-between gap-2">
        <h1 className="text-lg font-semibold">Pins</h1>
      </div>
      {!canConfigure && <LockNotice>{CONFIGURE_LOCK_MESSAGE}</LockNotice>}

      <Panel
        title={`Connectors (${board.name})`}
        action={
          placingFn && (
            <div className="flex items-center gap-2 text-sm">
              <span className="text-muted-foreground">Select the first pin for {placingFn.label}</span>
              <Button variant="ghost" onClick={() => setPlacing(null)}>
                Cancel
              </Button>
            </div>
          )
        }
      >
        <div className="overflow-x-auto">
          <div
            className="grid gap-2"
            style={{
              gridTemplateColumns: `repeat(${cols}, minmax(0, 1fr))`,
              gridTemplateRows: `repeat(${rows}, auto)`,
              minWidth: cols * connectorMinWidth + (cols - 1) * GRID_GAP_PX,
            }}
          >
            {board.connectors.map((c) => (
              <ConnectorView
                key={c.id}
                connector={c}
                label={label(c.id)}
                renaming={renaming === c.id}
                onRename={(name) => rename(c.id, name)}
                onCancelRename={() => setRenaming(null)}
                highlighted={active?.connector === c.id || c.pins.some((p) => "gpio" in p && pinHighlighted(p))}
                pinHighlighted={(p) => pinHighlighted(p)}
                used={used}
                placing={placing}
                map={map}
                preview={preview?.connector === c.id ? preview : null}
                onHover={setHover}
                onPreview={setPreview}
                onPinClick={(i, p) => onPinClick(c, i, p)}
                onConnectorClick={() => {
                  setMenu(null)
                  setSelected({ connector: c.id })
                  setRenaming(c.id)
                }}
              />
            ))}
          </div>
        </div>
        <PinInfo gpio={active?.gpio} used={used} label={label} />
        {menu && (
          <PinMenu
            title={pinTitle(board, menu.connector, menu.start, label)}
            functions={functionsAt(board, map, menu.connector, menu.start)}
            map={map}
            onPick={(fnId) => place(fnId, menu.connector, menu.start)}
            onClose={() => setMenu(null)}
          />
        )}
      </Panel>

      <Panel title="Functions">
        <div className="flex flex-col gap-2">
          {FUNCTIONS.map((fn) => (
            <FunctionRow
              key={fn.id}
              fn={fn}
              assignment={map[fn.id]}
              connectorLabel={map[fn.id] ? label(map[fn.id].connector) : ""}
              highlighted={fnHighlighted(fn)}
              placing={placing === fn.id}
              hasErrors={errors.some((e) => e.fn === fn.id)}
              onHover={setHover}
              onSelect={() => setSelected(selected?.fn === fn.id ? null : { fn: fn.id })}
              onPlace={() => {
                setMenu(null)
                setPlacing(placing === fn.id ? null : fn.id)
              }}
              onClear={() => clear(fn.id)}
              onEncoderType={(t) => update(fn.id, (a) => ({ ...a, encoderType: t }))}
              onOption={(i, o) =>
                update(fn.id, (a) => ({ ...a, options: a.options.map((x, j) => (j === i ? { ...x, ...o } : x)) }))
              }
            />
          ))}
        </div>
      </Panel>

      {issues.length > 0 && (
        <Panel title="Problems">
          <ul className="flex flex-col gap-1 text-sm">
            {issues.map((i, n) => (
              <li
                key={n}
                className="flex items-center gap-2"
                onMouseEnter={() => setHover({ fn: i.fn, gpio: i.gpio, connector: i.connector })}
                onMouseLeave={() => setHover(null)}
              >
                <Badge tone={i.level === "error" ? "bad" : "warn"}>{i.level === "error" ? "error" : "warning"}</Badge>
                {i.msg}
              </li>
            ))}
          </ul>
        </Panel>
      )}
    </div>
  )
}

function ConnectorView({
  connector: c,
  label,
  renaming,
  onRename,
  onCancelRename,
  highlighted,
  pinHighlighted,
  used,
  placing,
  map,
  preview,
  onHover,
  onPreview,
  onPinClick,
  onConnectorClick,
}: {
  connector: Connector
  label: string
  renaming: boolean
  onRename: (name: string) => void
  onCancelRename: () => void
  highlighted: boolean
  pinHighlighted: (p: BoardPin) => boolean
  used: Map<number, { fn: string; signal: string }[]>
  placing: string | null
  map: PinMap
  preview: Span | null
  onHover: (f: Focus | null) => void
  onPreview: (s: Span | null) => void
  onPinClick: (index: number, pin: BoardPin) => void
  onConnectorClick: () => void
}) {
  const board = useBoard()
  const placingLength = placing ? (FUNCTIONS.find((f) => f.id === placing)?.signals.length ?? 0) : 0
  return (
    <div
      data-keep-selection
      className={`flex flex-col gap-1.5 rounded-md border p-2 transition-colors ${
        highlighted ? "border-primary bg-primary/5" : "border-border"
      }`}
      style={{ gridRow: c.row, gridColumn: c.col }}
      onMouseEnter={() => onHover({ connector: c.id })}
      onMouseLeave={() => onHover(null)}
    >
      {renaming ? (
        <TextInput
          autoFocus
          defaultValue={label}
          placeholder={c.label}
          maxLength={CONNECTOR_NAME_MAX}
          className="h-6 w-full px-1.5 py-0 text-xs"
          onBlur={(e) => onRename(e.currentTarget.value)}
          onKeyDown={(e: KeyboardEvent<HTMLInputElement>) => {
            if (e.key === "Enter") {
              onRename(e.currentTarget.value)
            } else if (e.key === "Escape") {
              onCancelRename()
            }
          }}
        />
      ) : (
        <button
          type="button"
          title="Rename. Leave empty for the board's name."
          className="h-6 w-full truncate text-left text-xs font-medium text-muted-foreground hover:text-foreground"
          onClick={onConnectorClick}
        >
          {label}
        </button>
      )}
      <div className="flex gap-1">
        {c.pins.map((p, i) => {
          const isGpio = "gpio" in p
          const uses = isGpio ? (used.get(p.gpio) ?? []) : []
          const reserved = isGpio && p.gpio in board.reserved
          const strapping = isGpio && board.strapping.includes(p.gpio)
          const validStart = !!placing && canStartAt(board, map, placing, c.id, i)
          const inPreview = !!preview && i >= preview.start && i < preview.start + preview.length
          const conflict = uses.length > 1
          const fill = uses.length ? COLORS[colorIndex(uses[0].fn)] : ""
          const title = isGpio
            ? [
                `GPIO ${p.gpio}`,
                ...uses.map((u) => `${FUNCTIONS.find((f) => f.id === u.fn)?.label} ${u.signal}`),
                reserved ? `Reserved: ${board.reserved[p.gpio]}` : "",
                strapping ? `Strapping pin: ${STRAPPING_NOTE}` : "",
              ]
                .filter(Boolean)
                .join("\n")
            : p.power
          return (
            <button
              key={i}
              type="button"
              title={title}
              disabled={!isGpio && !placing}
              onMouseEnter={() => {
                onHover(isGpio ? { gpio: p.gpio } : { connector: c.id })
                onPreview(validStart ? { connector: c.id, start: i, length: placingLength } : null)
              }}
              onMouseLeave={() => {
                onHover({ connector: c.id })
                onPreview(null)
              }}
              onClick={() => onPinClick(i, p)}
              className={`relative flex h-12 min-w-8 flex-1 flex-col items-center justify-center rounded border text-xs tabular-nums transition-colors ${
                !isGpio
                  ? "border-dashed border-border text-muted-foreground"
                  : reserved
                    ? "cursor-not-allowed border-border bg-muted text-muted-foreground"
                    : fill || "border-border hover:bg-muted"
              } ${pinHighlighted(p) ? "ring-2 ring-primary" : ""} ${conflict ? "ring-2 ring-destructive" : ""} ${
                validStart ? "outline-2 outline-offset-1 outline-emerald-500 outline-dashed" : ""
              } ${inPreview ? "bg-emerald-500/30" : ""}`}
            >
              <span className={`font-medium ${reserved ? "line-through" : ""}`}>{isGpio ? p.gpio : p.power}</span>
              {uses.length > 0 && <span className="text-[10px] leading-none">{uses[0].signal}</span>}
              {strapping && <span className="absolute right-0.5 top-0 text-xs font-extrabold text-amber-600 dark:text-amber-400">!</span>}
              {reserved && <span className="absolute right-0.5 top-0 text-xs font-extrabold text-red-600 dark:text-red-400">!</span>}
            </button>
          )
        })}
      </div>
    </div>
  )
}

function PinInfo({
  gpio,
  used,
  label,
}: {
  gpio?: number
  used: Map<number, { fn: string; signal: string }[]>
  label: (id: string) => string
}) {
  const board = useBoard()
  const c = gpio === undefined ? undefined : board.connectors.find((x) => x.pins.some((p) => "gpio" in p && p.gpio === gpio))
  return (
    <p className="mt-3 min-h-10 border-t border-border pt-2 text-sm text-muted-foreground">
      {gpio === undefined || !c ? (
        "Hover or tap a pin for details."
      ) : (
        <>
          <span className="font-medium text-foreground">
            GPIO {gpio} ({label(c.id)})
          </span>
          {(used.get(gpio) ?? []).map((u) => (
            <span key={u.fn}>
              {" · "}
              {FUNCTIONS.find((f) => f.id === u.fn)?.label} {u.signal}
            </span>
          ))}
          {gpio in board.reserved && <span className="block">Reserved: {board.reserved[gpio]}.</span>}
          {board.strapping.includes(gpio) && <span className="block text-amber-600 dark:text-amber-400">Strapping pin: {STRAPPING_NOTE}.</span>}
          {!(gpio in board.reserved) && !(used.get(gpio)?.length) && <span> · free</span>}
        </>
      )}
    </p>
  )
}

function pinTitle(board: Board, connectorId: string, start: number, label: (id: string) => string) {
  const c = connectorOf(board, connectorId)
  const p = c?.pins[start]
  return `${label(connectorId)}, GPIO ${p && "gpio" in p ? p.gpio : "?"}`
}

function PinMenu({
  title,
  functions,
  map,
  onPick,
  onClose,
}: {
  title: string
  functions: FunctionDef[]
  map: PinMap
  onPick: (fnId: string) => void
  onClose: () => void
}) {
  return (
    <div data-keep-selection className="mt-3 flex flex-wrap items-center gap-2 border-t border-border pt-3 text-sm">
      <span className="text-muted-foreground">{title}:</span>
      {functions.length === 0 && <span className="text-muted-foreground">no function starts here.</span>}
      {functions.map((fn) => (
        <Button key={fn.id} onClick={() => onPick(fn.id)}>
          {map[fn.id] ? `Move ${fn.label} here` : fn.label}
        </Button>
      ))}
      <Button variant="ghost" onClick={onClose}>
        Cancel
      </Button>
    </div>
  )
}

function FunctionRow({
  fn,
  assignment: a,
  connectorLabel: cLabel,
  highlighted,
  placing,
  hasErrors,
  onHover,
  onSelect,
  onPlace,
  onClear,
  onEncoderType,
  onOption,
}: {
  fn: FunctionDef
  assignment: Assignment | undefined
  connectorLabel: string
  highlighted: boolean
  placing: boolean
  hasErrors: boolean
  onHover: (f: Focus | null) => void
  onSelect: () => void
  onPlace: () => void
  onClear: () => void
  onEncoderType: (t: EncoderType) => void
  onOption: (index: number, o: { invert?: boolean; pull?: Pull }) => void
}) {
  const board = useBoard()
  const [open, setOpen] = useState(false)
  const pins = a ? pinsOf(board, fn, a) : []
  return (
    <div
      data-keep-selection
      className={`rounded-md border p-2.5 transition-colors ${highlighted ? "border-primary bg-primary/5" : "border-border"}`}
      onMouseEnter={() => onHover({ fn: fn.id })}
      onMouseLeave={() => onHover(null)}
    >
      <div className="flex flex-wrap items-center gap-2">
        <button type="button" className="flex min-w-40 items-center gap-2 text-left text-sm font-medium" onClick={onSelect}>
          <span className={`h-2.5 w-2.5 shrink-0 rounded-full ${DOTS[colorIndex(fn.id)]}`} />
          {fn.label}
        </button>
        {!a && <Badge tone={fn.required ? "bad" : "neutral"}>{fn.required ? "required, no pins" : "not used"}</Badge>}
        {a && hasErrors && <Badge tone="bad">error</Badge>}
        {a && (
          <span className="text-sm text-muted-foreground">
            {cLabel}: {fn.signals.map((s, i) => `${s} ${pins[i] ?? "?"}`).join(" · ")}
          </span>
        )}
        <div className="ml-auto flex flex-wrap items-center gap-1">
          {fn.encoderTypes && a && (
            <Select
              value={a.encoderType}
              disabled={fn.encoderTypes.length < 2}
              onChange={(e) => onEncoderType(e.target.value as EncoderType)}
              title="hw: PCNT unit. sw: GPIO interrupts, for slow encoders only."
            >
              {fn.encoderTypes.map((t) => (
                <option key={t} value={t}>
                  {t}
                </option>
              ))}
            </Select>
          )}
          <Button variant={placing ? "primary" : "secondary"} onClick={onPlace}>
            {placing ? "Cancel" : a ? "Move" : "Place"}
          </Button>
          {a && <Button onClick={() => setOpen(!open)}>{open ? "Hide options" : "Options"}</Button>}
          {a && (
            <Button variant="ghost" onClick={onClear}>
              Clear
            </Button>
          )}
        </div>
      </div>
      {a && open && (
        <div className="mt-2 grid gap-2 border-t border-border pt-2 sm:grid-cols-2">
          {fn.signals.map((s, i) => (
            <div key={s} className="flex flex-wrap items-center gap-2 text-sm">
              <span className="w-16 font-medium">
                {s} {pins[i] ?? "?"}
              </span>
              <span className="flex items-center gap-1.5 text-muted-foreground">
                <Toggle checked={a.options[i]?.invert ?? false} onChange={(v) => onOption(i, { invert: v })} label="Invert" />
                invert
              </span>
              {fn.dir === "in" && (
                <Select value={a.options[i]?.pull ?? "none"} onChange={(e) => onOption(i, { pull: e.target.value as Pull })}>
                  <option value="none">no pull</option>
                  <option value="up">pull-up</option>
                  <option value="down">pull-down</option>
                </Select>
              )}
            </div>
          ))}
        </div>
      )}
    </div>
  )
}
