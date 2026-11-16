"use client"

import { useEffect, useRef, useState, type ReactNode } from "react"
import { createPortal } from "react-dom"
import { X } from "lucide-react"
import { STRAPPING_NOTE, assignmentLabel, connectorLabel, functionDef, pinsOf, usage, type Board, type ConnectorNames, type Issue, type PinMap } from "../pins"
import { Button, IconButton } from "../ui/primitives"

const SHEET_COLORS = [
  "bg-sky-500/25 border-sky-600",
  "bg-violet-500/25 border-violet-600",
  "bg-emerald-500/25 border-emerald-600",
  "bg-amber-500/25 border-amber-600",
  "bg-rose-500/25 border-rose-600",
  "bg-teal-500/25 border-teal-600",
]

function colorOf(board: Board, fnId: string) {
  return SHEET_COLORS[Math.max(0, board.functions.findIndex((f) => f.id === fnId)) % SHEET_COLORS.length]
}

export function PinsSheet({
  board,
  map,
  names,
  issues,
  onClose,
}: {
  board: Board
  map: PinMap
  names: ConnectorNames
  issues: Issue[]
  onClose: () => void
}) {
  const [colors, setColors] = useState(true)
  const [details, setDetails] = useState(true)
  const container = useRef<HTMLDivElement | null>(null)
  if (!container.current && typeof document !== "undefined") {
    container.current = document.createElement("div")
  }

  useEffect(() => {
    const el = container.current
    if (!el) {
      return
    }
    const hidden = [...document.body.children].filter((c) => !c.classList.contains("print:hidden"))
    hidden.forEach((c) => c.classList.add("print:hidden"))
    document.body.appendChild(el)
    const onKeyDown = (e: KeyboardEvent) => e.key === "Escape" && onClose()
    document.addEventListener("keydown", onKeyDown)
    return () => {
      document.removeEventListener("keydown", onKeyDown)
      el.remove()
      hidden.forEach((c) => c.classList.remove("print:hidden"))
    }
  }, [onClose])

  if (!container.current) {
    return null
  }
  return createPortal(
    <div className="fixed inset-0 z-50 overflow-auto bg-neutral-500/60 print:static print:overflow-visible print:bg-white">
      <div className="sticky top-0 z-10 border-b border-border bg-card text-sm text-foreground print:hidden">
        <div className="mx-auto flex max-w-5xl flex-wrap items-center gap-4 px-4 py-2">
          <span className="font-semibold">Print sheet</span>
          <label className="flex items-center gap-1.5">
            <input type="checkbox" checked={colors} onChange={(e) => setColors(e.target.checked)} />
            Colors
          </label>
          <label className="flex items-center gap-1.5">
            <input type="checkbox" checked={details} onChange={(e) => setDetails(e.target.checked)} />
            Details
          </label>
          <div className="ml-auto flex items-center gap-2">
            <Button variant="primary" onClick={() => window.print()}>
              Print
            </Button>
            <IconButton icon={X} title="Close" onClick={onClose} />
          </div>
        </div>
      </div>
      <div
        className="mx-auto my-6 max-w-5xl bg-white p-8 text-neutral-900 shadow-lg print:m-0 print:max-w-none print:p-0 print:shadow-none"
        style={{ printColorAdjust: "exact", WebkitPrintColorAdjust: "exact" }}
      >
        <SheetHeader board={board} />
        <SheetConnectors board={board} map={map} names={names} colors={colors} />
        {details && <SheetDetails board={board} map={map} names={names} issues={issues} />}
      </div>
    </div>,
    container.current,
  )
}

function SheetHeader({ board }: { board: Board }) {
  return (
    <div className="mb-4 flex items-baseline justify-between border-b border-neutral-400 pb-2">
      <h1 className="text-lg font-semibold">Pin map, {board.name}</h1>
      <span className="text-sm">{new Date().toLocaleDateString()}</span>
    </div>
  )
}

function SheetConnectors({ board, map, names, colors }: { board: Board; map: PinMap; names: ConnectorNames; colors: boolean }) {
  const used = usage(board, map)
  const rows = Math.max(...board.connectors.map((c) => c.row))
  const cols = Math.max(...board.connectors.map((c) => c.col))
  return (
    <div className="grid gap-2" style={{ gridTemplateColumns: `repeat(${cols}, minmax(0, 1fr))`, gridTemplateRows: `repeat(${rows}, auto)` }}>
      {board.connectors.map((c) => (
        <div key={c.id} className="flex break-inside-avoid flex-col gap-1 rounded border border-neutral-400 p-1.5" style={{ gridRow: c.row, gridColumn: c.col }}>
          <span className="truncate text-xs font-semibold">{connectorLabel(board, names, c.id)}</span>
          <div className="grid gap-0.5" style={{ gridTemplateColumns: `repeat(${c.pins.length}, minmax(0, 1fr))` }}>
            {c.pins.map((p, i) => {
              const uses = "gpio" in p ? (used.get(p.gpio) ?? []) : []
              const text = "power" in p ? p.power : uses.length ? uses.map((u) => u.signal).join(" / ") : "-"
              const fill = colors && uses.length ? colorOf(board, uses[0].fn) : "border-neutral-400"
              return (
                <div
                  key={i}
                  className={`flex min-h-8 items-center justify-center rounded-sm border px-0.5 text-center text-[9px] leading-tight font-medium uppercase ${fill} ${
                    "power" in p ? "border-dashed" : ""
                  }`}
                  style={{ gridRow: 1, gridColumn: i + 1 }}
                >
                  {text}
                </div>
              )
            })}
            {map.flatMap((a, index) => {
              if (a.connector !== c.id) {
                return []
              }
              const fill = colors ? colorOf(board, a.fn) : "border-neutral-400"
              return [
                <div
                  key={`fn-${index}`}
                  className={`rounded-sm border px-0.5 py-0.5 text-center text-[9px] leading-tight font-semibold uppercase ${fill}`}
                  style={{ gridColumn: `${a.start + 1} / span ${functionDef(board, a.fn)?.signals.length ?? 1}` }}
                >
                  {assignmentLabel(board, map, index)}
                </div>,
              ]
            })}
          </div>
        </div>
      ))}
    </div>
  )
}

function SheetDetails({ board, map, names, issues }: { board: Board; map: PinMap; names: ConnectorNames; issues: Issue[] }) {
  const rows = board.functions.flatMap((fn) => map.flatMap((a, index) => (a.fn === fn.id ? [{ fn, a, index }] : [])))
  return (
    <table className="mt-6 w-full border-collapse text-xs">
      <thead>
        <tr className="border-b border-neutral-400 text-left">
          <Th>Function</Th>
          <Th>Connector</Th>
          <Th>Signal</Th>
          <Th>GPIO</Th>
          <Th>Invert</Th>
          <Th>Pull</Th>
          <Th>Notes</Th>
        </tr>
      </thead>
      {rows.map(({ fn, a, index }) => {
        const pins = pinsOf(board, fn, a)
        const fnNotes = [
          a.encoderType ? `${a.encoderType}, ${a.encoderType === "hw" ? "glitch filter" : "debounce"} ${a.filterUs} µs` : "",
          ...issues.filter((i) => i.index === index || (i.fn === fn.id && i.index === undefined)).map((i) => `${i.level}: ${i.msg}`),
        ].filter(Boolean)
        return (
          <tbody key={index} className="break-inside-avoid border-b border-neutral-300">
            {fn.signals.map((s, i) => {
              const gpio = pins[i] ?? undefined
              const pinNotes = [
                gpio !== undefined && gpio in board.reserved ? `reserved: ${board.reserved[gpio]}` : "",
                gpio !== undefined && board.strapping.includes(gpio) ? `strapping pin: ${STRAPPING_NOTE}` : "",
              ].filter(Boolean)
              return (
                <tr key={s} className="align-top">
                  {i === 0 && (
                    <>
                      <Td rowSpan={fn.signals.length} className="font-medium">
                        {assignmentLabel(board, map, index)}
                      </Td>
                      <Td rowSpan={fn.signals.length}>{connectorLabel(board, names, a.connector)}</Td>
                    </>
                  )}
                  <Td>{s}</Td>
                  <Td>{gpio ?? "?"}</Td>
                  <Td>{a.options[i]?.invert ? "yes" : "no"}</Td>
                  <Td>{fn.dir === "in" ? (a.options[i]?.pull ?? "none") : ""}</Td>
                  <Td>{[...(i === 0 ? fnNotes : []), ...pinNotes].join("; ")}</Td>
                </tr>
              )
            })}
          </tbody>
        )
      })}
    </table>
  )
}

function Th({ children }: { children: ReactNode }) {
  return <th className="px-1.5 py-1 font-semibold">{children}</th>
}

function Td({ children, rowSpan, className = "" }: { children: ReactNode; rowSpan?: number; className?: string }) {
  return (
    <td rowSpan={rowSpan} className={`px-1.5 py-1 ${className}`}>
      {children}
    </td>
  )
}
