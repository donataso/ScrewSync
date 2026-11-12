"use client"

import { useEffect, useMemo, useRef, useState } from "react"
import {
  deleteGcode,
  fetchGcodeActiveText,
  fetchGcodeList,
  fetchGcodeProgramText,
  selectGcode,
  uploadDisplayPreview,
  updateGcode,
  uploadGcode,
  type GcodeListResult,
  type GcodeProgramInfo,
} from "../gcode/api"
import { axesMismatched, flipMismatchedAxes, parseAxesHeader } from "../gcode/axes"
import { interpretLine, type Step, type StepSink } from "../gcode/core"
import { detectFlavor, find, list } from "../gcode/flavors"
import { markLathecodeTravel } from "../gcode/lathecode"
import { CANVAS_H, CANVAS_W, canvasToJpeg, computeMapping, drawPreview } from "../gcode/preview"
import { useRouter } from "../router"
import { CONFIGURE_LOCK_MESSAGE, useCanConfigure } from "../state-provider"
import { sendCommand } from "../api"
import { cmd } from "../commands"
import { Button, Field, LockNotice, Panel, Select, TextInput } from "../ui/primitives"
import { useToast } from "../ui/toast"

const LATHECODE_URL = "https://kachurovskiy.com/lathecode/"
const ROUTE = "/gcode"

// The "id" segment of "/gcode/<id>", else null.
function idFromPath(path: string): string | null {
  return path.startsWith(ROUTE + "/") ? decodeURIComponent(path.slice(ROUTE.length + 1)) : null
}

interface ParseResult {
  steps: Step[]
  error: { line: number; message: string } | null
}

function parseGcode(text: string, flavorId: string): ParseResult {
  const flavor = find(flavorId)
  if (!flavor) return { steps: [], error: { line: 0, message: "unknown flavor" } }
  flavor.begin(0, 0)
  const steps: Step[] = []
  const sink: StepSink = { emit: (s) => steps.push(s) }
  const lines = text.split("\n")
  for (let i = 0; i < lines.length; i++) {
    const err = interpretLine(flavor, lines[i], i + 1, sink)
    if (err) return { steps, error: { line: i + 1, message: err } }
  }
  return { steps, error: null }
}

function stripExtension(filename: string): string {
  const i = filename.lastIndexOf(".")
  return i > 0 ? filename.slice(0, i) : filename
}

function timestampName(): string {
  const d = new Date()
  const pad = (n: number) => String(n).padStart(2, "0")
  return `${d.getFullYear()}-${pad(d.getMonth() + 1)}-${pad(d.getDate())} ${pad(d.getHours())}:${pad(d.getMinutes())}`
}

export function GcodeScreen() {
  const [sourceMode, setSourceMode] = useState<"file" | "paste">("paste")
  const [file, setFile] = useState<File | null>(null)
  const [fileText, setFileText] = useState("")
  const [pasteText, setPasteText] = useState("")
  const [name, setName] = useState("")
  const [nameTouched, setNameTouched] = useState(false)
  const [flavorId, setFlavorId] = useState(() => list()[0]?.id() ?? "")
  const [flavorTouched, setFlavorTouched] = useState(false)
  const [busy, setBusy] = useState(false)
  const [listResult, setListResult] = useState<GcodeListResult | null>(null)
  const [loading, setLoading] = useState(false)
  // Text of the saved program open in the form, to tell whether it was edited.
  const [savedText, setSavedText] = useState<string | null>(null)
  const [savedName, setSavedName] = useState<string | null>(null)
  const [savedFlavor, setSavedFlavor] = useState<string | null>(null)
  const [openedId, setOpenedId] = useState<string | null>(null)
  const fileInputRef = useRef<HTMLInputElement>(null)
  const canvasRef = useRef<HTMLCanvasElement>(null)
  const canConfigure = useCanConfigure()
  const toast = useToast()
  const { path, navigate } = useRouter()

  const text = sourceMode === "file" ? fileText : pasteText
  const parsed = useMemo(() => parseGcode(text, flavorId), [text, flavorId])
  const extents = useMemo(() => (parsed.error ? null : computeMapping(parsed.steps)), [parsed])
  const axesHeader = useMemo(() => parseAxesHeader(text), [text])
  const axesWrong = !!axesHeader && axesMismatched(axesHeader)

  function openProgram(programText: string, programName: string, programFlavorId: string, savedId: string | null) {
    setSourceMode("paste")
    setFile(null)
    setFileText("")
    setPasteText(programText)
    setName(programName)
    setNameTouched(true)
    setFlavorId(programFlavorId)
    setFlavorTouched(true)
    setSavedText(programText)
    setSavedName(programName)
    setSavedFlavor(programFlavorId)
    setOpenedId(savedId)
    renderPreview(programText, programFlavorId)
  }

  async function load(openActive = false): Promise<GcodeListResult | null> {
    setLoading(true)
    try {
      const r = await fetchGcodeList()
      setListResult(r)
      if (openActive && r.active?.id) {
        const activeText = await fetchGcodeActiveText()
        if (activeText !== null) openProgram(activeText, r.active.name, r.active.flavor, r.active.id)
      }
      return r
    } catch (e) {
      toast.push(`Could not load G-code list: ${(e as Error).message}`, "bad")
      return null
    } finally {
      setLoading(false)
    }
  }

  // Initial load: the active program, unless the URL names a saved one
  // (a refresh), in which case the URL effect below opens that instead.
  useEffect(() => {
    if (!idFromPath(path)) load(true)
    // eslint-disable-next-line react-hooks/exhaustive-deps
  }, [])

  // Keeps the form in sync with "/gcode" or "/gcode/<id>" in the URL,
  // for a page refresh and for back/forward through picked programs.
  useEffect(() => {
    const id = idFromPath(path)
    if (!id) {
      if (openedId !== null) {
        setSourceMode("paste")
        setFile(null)
        setFileText("")
        setPasteText("")
        setName(timestampName())
        setNameTouched(false)
        setFlavorTouched(false)
        setSavedText(null)
        setSavedName(null)
        setSavedFlavor(null)
        setOpenedId(null)
      }
      return
    }
    if (id === openedId) return
    setBusy(true)
    ;(async () => {
      try {
        const programText = await fetchGcodeProgramText(id)
        const listRes = listResult ?? (await fetchGcodeList())
        if (!listResult) setListResult(listRes)
        const info = listRes.programs.find((p) => p.id === id)
        openProgram(programText, info?.name ?? "", info?.flavor ?? flavorId, id)
      } catch (e) {
        toast.push(`Could not open: ${(e as Error).message}`, "bad")
      } finally {
        setBusy(false)
      }
    })()
    // eslint-disable-next-line react-hooks/exhaustive-deps
  }, [path])

  useEffect(() => {
    if (!text || flavorTouched) return
    const detected = detectFlavor(text)
    if (detected) setFlavorId(detected)
  }, [text, flavorTouched])

  useEffect(() => {
    if (nameTouched) return
    if (sourceMode === "file" && file) setName(stripExtension(file.name))
    else if (sourceMode === "paste") setName(timestampName())
  }, [sourceMode, file, nameTouched])

  useEffect(() => {
    if (text) renderPreview(text, flavorId)
  }, [text, flavorId])

  function renderPreview(sourceText: string, forFlavorId: string) {
    const canvas = canvasRef.current
    if (!canvas) return
    const ctx = canvas.getContext("2d")
    if (!ctx) return
    const { steps } = parseGcode(sourceText, forFlavorId)
    drawPreview(ctx, forFlavorId === "lathecode" ? markLathecodeTravel(sourceText, steps) : steps)
  }

  async function sendPreviewToDisplay() {
    const canvas = canvasRef.current
    if (!canvas) return
    const blob = await canvasToJpeg(canvas)
    if (!blob) {
      toast.push("Preview image too large for the display, even at low quality.", "warn")
      return
    }
    if (!extents) return
    const result = await uploadDisplayPreview(blob, extents)
    if (!result.ok) toast.push(`Display preview not sent: ${result.message}`, "warn")
  }

  function fixAxes() {
    const fixed = flipMismatchedAxes(text)
    if (sourceMode === "file") setFileText(fixed)
    else setPasteText(fixed)
  }

  function onPickFile(f: File | null) {
    setFile(f)
    setOpenedId(null)
    setNameTouched(false)
    navigate(ROUTE)
    if (!f) {
      setFileText("")
      return
    }
    f.text()
      .then(setFileText)
      .catch((e) => toast.push(`Could not read file: ${(e as Error).message}`, "bad"))
  }

  async function save() {
    if (!text || parsed.error || !name.trim()) return
    setBusy(true)
    try {
      const trimmedName = name.trim()
      const result =
        openedId !== null
          ? await updateGcode(openedId, {
              name: trimmedName !== savedName ? trimmedName : undefined,
              flavor: flavorId !== savedFlavor ? flavorId : undefined,
              text: text !== savedText ? text : undefined,
            })
          : await uploadGcode(text, trimmedName, flavorId, "save")
      if (!result.ok) {
        toast.push(`Save rejected: ${result.error}`, "bad")
        return
      }
      toast.push("Saved.", "good")
      setSavedText(text)
      setSavedName(trimmedName)
      setSavedFlavor(flavorId)
      setName(trimmedName)
      setOpenedId(result.id)
      if (result.id) navigate(`${ROUTE}/${encodeURIComponent(result.id)}`)
      await load()
    } catch (e) {
      toast.push(`Save failed: ${(e as Error).message}`, "bad")
    } finally {
      setBusy(false)
    }
  }

  // Makes the form's program the one the lathe runs, and shows it on the display.
  async function runOnLathe() {
    if (!text || parsed.error || !name.trim()) return
    setBusy(true)
    try {
      const unchangedSaved = openedId !== null && !changed
      const result = unchangedSaved
        ? await selectGcode(openedId)
        : await uploadGcode(text, name.trim(), flavorId, "run")
      if (!result.ok) {
        toast.push(`Rejected: ${result.error}`, "bad")
        return
      }
      renderPreview(text, flavorId)
      await sendPreviewToDisplay()
      const outcome = await sendCommand(cmd.mode("gcode"))
      if (outcome.status === "denied") toast.push(`Display not switched to Gcode mode: ${outcome.msg}`, "warn")
      else toast.push("Loaded on the lathe.", "good")
      await load()
    } catch (e) {
      toast.push(`Load failed: ${(e as Error).message}`, "bad")
    } finally {
      setBusy(false)
    }
  }

  async function openSaved(p: GcodeProgramInfo) {
    setBusy(true)
    try {
      const programText = await fetchGcodeProgramText(p.id)
      openProgram(programText, p.name, p.flavor, p.id)
      navigate(`${ROUTE}/${encodeURIComponent(p.id)}`)
    } catch (e) {
      toast.push(`Could not open: ${(e as Error).message}`, "bad")
    } finally {
      setBusy(false)
    }
  }

  async function deleteProgram(p: GcodeProgramInfo) {
    if (!window.confirm(`Delete saved program "${p.name}"?`)) return
    setBusy(true)
    try {
      const result = await deleteGcode(p.id)
      if (result.ok) {
        toast.push("Deleted.", "good")
        setOpenedId(null)
        if (p.id === openedId) navigate(ROUTE)
        await load()
      } else {
        toast.push(`Delete rejected: ${result.message}`, "bad")
      }
    } finally {
      setBusy(false)
    }
  }

  const openedProgram = listResult?.programs.find((p) => p.id === openedId) ?? null
  const showFlavor = list().length > 1
  const valid = !!text && !parsed.error && !!name.trim() && !!flavorId
  const changed = text !== savedText || name.trim() !== savedName || flavorId !== savedFlavor
  const canSave = canConfigure && !busy && valid && !axesWrong && (openedId === null || changed)
  const canRun = canConfigure && !busy && valid && !axesWrong

  return (
    <div className="flex flex-col gap-4">
      <div className="flex items-center justify-between">
        <h1 className="text-lg font-semibold">Gcode</h1>
        <a
          href={LATHECODE_URL}
          target="_blank"
          rel="noreferrer"
          className="text-sm text-sky-600 underline hover:no-underline dark:text-sky-400"
        >
          Design or slice in LatheCode
        </a>
      </div>

      {!canConfigure && <LockNotice>{CONFIGURE_LOCK_MESSAGE}</LockNotice>}

      <div className="grid items-start gap-4 lg:grid-cols-[16rem_1fr]">
        <Panel title="Saved programs">
          {loading && !listResult ? (
            <p className="text-sm text-muted-foreground">Loading...</p>
          ) : !listResult || listResult.programs.length === 0 ? (
            <p className="text-sm text-muted-foreground">No saved programs.</p>
          ) : (
            <div className="-mx-2 flex flex-col">
              {listResult.programs.map((p) => (
                <button
                  key={p.id}
                  type="button"
                  disabled={busy}
                  onClick={() => openSaved(p)}
                  className={`flex items-center gap-2 rounded-md px-2 py-1.5 text-left text-sm transition-colors hover:bg-muted/60 disabled:cursor-not-allowed disabled:opacity-60 ${
                    p.id === openedId ? "bg-muted font-medium" : ""
                  }`}
                >
                  <span className="flex-1 truncate">{p.name}</span>
                  {listResult.active?.id === p.id && (
                    <span className="rounded-full bg-emerald-500/15 px-2 py-0.5 text-xs font-medium text-emerald-500 dark:text-emerald-400">
                      active
                    </span>
                  )}
                </button>
              ))}
            </div>
          )}
          {listResult && (
            <p className="mt-3 text-xs text-muted-foreground">{(listResult.free / 1024).toFixed(0)} KiB free</p>
          )}
        </Panel>

        <div className="flex min-w-0 flex-col gap-4">
          <Panel title="Program">
            <div className="flex flex-col gap-3">
              <div className="flex gap-2">
                <Button variant={sourceMode === "paste" ? "primary" : "secondary"} onClick={() => setSourceMode("paste")}>
                  Paste
                </Button>
                <Button variant={sourceMode === "file" ? "primary" : "secondary"} onClick={() => setSourceMode("file")}>
                  File
                </Button>
              </div>

              {sourceMode === "file" ? (
                <div className="flex flex-wrap items-center gap-2">
                  <Button onClick={() => fileInputRef.current?.click()}>Choose file</Button>
                  <input
                    ref={fileInputRef}
                    type="file"
                    accept=".gcode,.nc,.txt"
                    className="hidden"
                    onChange={(e) => onPickFile(e.target.files?.[0] ?? null)}
                  />
                  {file && <span className="text-sm text-muted-foreground">{file.name}</span>}
                </div>
              ) : (
                <textarea
                  value={pasteText}
                  onChange={(e) => setPasteText(e.target.value)}
                  placeholder="Paste G-code here"
                  rows={10}
                  className="w-full rounded-md border border-input bg-background p-2.5 font-mono text-xs outline-none focus:ring-2 focus:ring-ring"
                />
              )}

              <div className="grid gap-3 sm:grid-cols-3">
                <div className={showFlavor ? "sm:col-span-2" : "sm:col-span-3"}>
                  <Field label="Name">
                    <TextInput
                      value={name}
                      maxLength={63}
                      onChange={(e) => {
                        setName(e.target.value)
                        setNameTouched(true)
                      }}
                    />
                  </Field>
                </div>
                {showFlavor && (
                  <Field label="Flavor">
                    <Select
                      value={flavorId}
                      onChange={(e) => {
                        setFlavorId(e.target.value)
                        setFlavorTouched(true)
                      }}
                    >
                      {list().map((f) => (
                        <option key={f.id()} value={f.id()}>
                          {f.id()}
                        </option>
                      ))}
                    </Select>
                  </Field>
                )}
              </div>

              {axesHeader && axesWrong && (
                <div className="flex flex-wrap items-center gap-3 rounded-md border-2 border-red-500 bg-red-50 px-3 py-2 text-sm text-red-800 dark:border-red-500 dark:bg-red-950/40 dark:text-red-300">
                  <p className="flex-1">
                    This lathe needs <strong>Z+ right, X+ down</strong>; this program's header says Z+ points{" "}
                    <strong>{axesHeader.zDir}</strong>, X+ points <strong>{axesHeader.xDir}</strong>. Set{" "}
                    <strong>AXES RIGHT DOWN</strong> in LatheCode (Axes setup) and regenerate, or flip it here.
                  </p>
                  <Button variant="danger" onClick={fixAxes}>
                    Flip axes
                  </Button>
                </div>
              )}

              {text &&
                (parsed.error ? (
                  <p className="text-sm text-destructive">
                    Line {parsed.error.line}: {parsed.error.message}
                  </p>
                ) : (
                  <p className="text-sm text-muted-foreground">
                    {parsed.steps.length} step(s). Z {extents?.zMinMm.toFixed(2)} to {extents?.zMaxMm.toFixed(2)} mm, X{" "}
                    {extents?.xMinMm.toFixed(2)} to {extents?.xMaxMm.toFixed(2)} mm.
                  </p>
                ))}

              <div className="flex gap-2">
                <Button variant="primary" disabled={!canRun} onClick={runOnLathe}>
                  Run on lathe
                </Button>
                <Button disabled={!canSave} onClick={save}>
                  Save
                </Button>
                {openedProgram && (
                  <Button
                    variant="danger"
                    disabled={busy || !canConfigure}
                    onClick={() => deleteProgram(openedProgram)}
                    className="ml-auto"
                  >
                    Delete saved
                  </Button>
                )}
              </div>
            </div>
          </Panel>

          {/* Kept mounted so the canvas can be drawn before it is shown. */}
          <Panel title="Preview" className={text ? "" : "hidden"}>
            <canvas
              ref={canvasRef}
              width={CANVAS_W}
              height={CANVAS_H}
              style={{ aspectRatio: `${CANVAS_W} / ${CANVAS_H}` }}
              className="w-full max-w-xl rounded-md border border-border"
            />
          </Panel>
        </div>
      </div>
    </div>
  )
}
