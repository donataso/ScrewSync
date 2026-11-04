"use client"

import { useState } from "react"
import { degreesToRatio, formatDegrees, formatRatioN, taperError } from "../threads"
import { Field, TextInput } from "./primitives"

type Side = "deg" | "ratio"

// Signed diameter-per-length ratio. With `inherited`, empty = undefined (use
// the inherited value). Remount (key) to load a different value.
export function TaperInput({
  value,
  inherited,
  onChange,
}: {
  value: number | undefined
  inherited?: number
  onChange: (ratio: number | undefined) => void
}) {
  const inherits = inherited !== undefined
  const [deg, setDeg] = useState(value === undefined ? "" : formatDegrees(value))
  const [ratioN, setRatioN] = useState(value === undefined ? "" : formatRatioN(value))
  const [dir, setDir] = useState(value !== undefined && value < 0 ? -1 : 1)
  const [lastEdited, setLastEdited] = useState<Side>("deg")
  const shown = value ?? inherited ?? 0

  function empty() {
    setDeg("")
    setRatioN("")
    onChange(inherits ? undefined : 0)
  }

  function editDeg(text: string) {
    setLastEdited("deg")
    if (text.trim() === "") return empty()
    setDeg(text)
    const d = Number(text)
    if (Number.isNaN(d) || d < 0) return onChange(NaN)
    const ratio = d >= 90 ? Infinity : dir * degreesToRatio(d)
    setRatioN(Number.isFinite(ratio) ? formatRatioN(ratio) : "")
    onChange(ratio)
  }

  function editRatio(text: string) {
    setLastEdited("ratio")
    if (text.trim() === "") return empty()
    setRatioN(text)
    const n = Number(text)
    if (!(n > 0)) return onChange(NaN)
    const ratio = dir / n
    setDeg(formatDegrees(ratio))
    onChange(ratio)
  }

  function blur(side: Side) {
    if (value === undefined || !Number.isFinite(value)) return
    if (side === "deg") setDeg(formatDegrees(value))
    else setRatioN(formatRatioN(value))
  }

  function setDirection(next: number) {
    setDir(next)
    if (value !== undefined && Number.isFinite(value) && Math.sign(value) !== next) onChange(-value)
  }

  const error = taperError(value)
  return (
    <div className="flex flex-col gap-2">
      <div className="grid grid-cols-2 gap-3">
        <Field label="Taper half-angle (°)" error={lastEdited === "deg" ? error : undefined}>
          <TextInput
            inputMode="decimal"
            value={deg}
            placeholder={inherits ? formatDegrees(inherited) : "0"}
            onChange={(e) => editDeg(e.target.value)}
            onBlur={() => blur("deg")}
          />
        </Field>
        <Field label="Taper ratio 1:N" error={lastEdited === "ratio" ? error : undefined}>
          <TextInput
            inputMode="decimal"
            value={ratioN}
            placeholder={inherits ? formatRatioN(inherited) || "none" : "none"}
            onChange={(e) => editRatio(e.target.value)}
            onBlur={() => blur("ratio")}
          />
        </Field>
      </div>
      {value !== undefined && Number.isFinite(value) && value !== 0 && (
        <div className="flex flex-wrap gap-1" role="radiogroup" aria-label="Taper direction">
          {[
            { d: 1, label: "Grows toward chuck" },
            { d: -1, label: "Shrinks toward chuck" },
          ].map((o) => (
            <button
              key={o.d}
              type="button"
              role="radio"
              aria-checked={dir === o.d}
              onClick={() => setDirection(o.d)}
              className={`whitespace-nowrap rounded-md border border-border px-2.5 py-1 text-xs font-medium transition-colors ${
                dir === o.d ? "bg-primary text-primary-foreground" : "text-muted-foreground hover:bg-muted"
              }`}
            >
              {o.label}
            </button>
          ))}
        </div>
      )}
      {value === undefined && inherits && shown !== 0 && (
        <p className="text-xs text-muted-foreground">Group taper, {shown > 0 ? "grows" : "shrinks"} toward chuck</p>
      )}
    </div>
  )
}
