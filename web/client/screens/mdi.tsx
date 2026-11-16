"use client"

import { useEffect, useMemo, useRef, useState, type ClipboardEvent, type KeyboardEvent } from "react"
import { sendCommand } from "../api"
import { addToHistory, freshRecall, loadHistory, saveHistory, splitLines, stepHistory, type Recall } from "../mdi-history"
import { useDeviceState } from "../state-provider"
import { MdiQueue, type Entry } from "../mdi-queue"
import { wsSubscribe } from "../ws"
import { Button, Panel, Select, TextInput } from "../ui/primitives"

function statusText(e: Entry): string {
  if (e.status === "sending") {
    return "sending"
  }
  if (e.status === "replied" && e.ok && e.stage) {
    return e.stage
  }
  return e.status === "replied" ? e.reply : e.status
}

function statusClass(e: Entry): string {
  if (e.status !== "replied") {
    return "text-muted-foreground"
  }
  if (!e.ok) {
    return "text-destructive"
  }
  if (e.stage === "running") {
    return "text-amber-500 dark:text-amber-400"
  }
  return e.stage === "accepted" ? "text-muted-foreground" : "text-emerald-500 dark:text-emerald-400"
}

const WCS_WORDS = ["G54", "G55", "G56", "G57", "G58", "G59"]

export function MdiScreen() {
  const { state, capabilities } = useDeviceState()
  const [history, setHistory] = useState<string[]>(() => loadHistory())
  const [recall, setRecall] = useState<Recall>(() => freshRecall(history))
  const [input, setInput] = useState("")
  const [log, setLog] = useState<Entry[]>([])
  const logEnd = useRef<HTMLDivElement>(null)
  const queue = useMemo(() => new MdiQueue(sendCommand, setLog), [])
  const homed = !!state && state.z.homed && (!capabilities?.has_x_motor || state.x.homed)

  useEffect(() => {
    logEnd.current?.scrollIntoView({ block: "nearest" })
  }, [log])

  useEffect(() => wsSubscribe({ onPrint: (text) => queue.addPrint(text) }), [queue])

  useEffect(() => {
    if (state) {
      queue.setFree(state.mdiFree, state.mdiDone)
    }
  }, [queue, state])

  function onPaste(e: ClipboardEvent<HTMLInputElement>) {
    const lines = splitLines(e.clipboardData.getData("text"))
    if (lines.length < 2) {
      return
    }
    e.preventDefault()
    let next = history
    for (const line of lines) {
      next = addToHistory(next, line)
    }
    setHistory(next)
    saveHistory(next)
    setRecall(freshRecall(next))
    setInput("")
    queue.enqueue(lines)
  }

  function submit() {
    const line = input.trim()
    if (!line) {
      return
    }
    const next = addToHistory(history, line)
    setHistory(next)
    saveHistory(next)
    setRecall(freshRecall(next))
    setInput("")
    queue.enqueue([line])
  }

  function onKeyDown(e: KeyboardEvent<HTMLInputElement>) {
    if (e.key === "Enter") {
      e.preventDefault()
      submit()
    } else if (e.key === "ArrowUp" || e.key === "ArrowDown") {
      e.preventDefault()
      const step = stepHistory(history, recall, input, e.key === "ArrowUp" ? -1 : 1)
      setRecall(step.recall)
      setInput(step.text)
    } else if (e.key === "Escape") {
      setInput("")
      setRecall(freshRecall(history))
    }
  }

  return (
    <Panel
      title="MDI"
      action={
        <div className="flex gap-2">
          <Button onClick={() => queue.clear()} disabled={!log.length}>
            Clear
          </Button>
          <Button variant="danger" onClick={() => queue.sendNow("stop")}>
            Stop
          </Button>
        </div>
      }
    >
      {state && (
        <div className="mb-2 flex flex-wrap items-center gap-2">
          {state.state === "estop" ? (
            <Button variant="danger" onClick={() => queue.sendNow("estop off")}>
              EStop on
            </Button>
          ) : (
            <Button onClick={() => queue.sendNow("estop on")}>EStop off</Button>
          )}
          <Button variant={state.z.enabled ? "primary" : "secondary"} onClick={() => queue.sendNow(`power ${state.z.enabled ? "off" : "on"} z`)}>
            Z motor {state.z.enabled ? "on" : "off"}
          </Button>
          {capabilities?.has_x_motor && (
            <Button variant={state.x.enabled ? "primary" : "secondary"} onClick={() => queue.sendNow(`power ${state.x.enabled ? "off" : "on"} x`)}>
              X motor {state.x.enabled ? "on" : "off"}
            </Button>
          )}
          {capabilities?.has_endstops && (
            <Button
              variant={homed ? "primary" : "secondary"}
              disabled={state.homing}
              onClick={() => queue.enqueue([homed ? "unhome" : "home"])}
            >
              {state.homing ? "Homing" : homed ? "Homed" : "Not homed"}
            </Button>
          )}
          {capabilities?.has_endstops && (
            <Select
              className="ml-auto"
              aria-label="Work coordinate system"
              value={state.wcs ?? "G54"}
              onChange={(e) => {
                queue.enqueue([e.target.value])
              }}
            >
              {WCS_WORDS.map((word) => (
                <option key={word} value={word}>
                  {word}
                </option>
              ))}
            </Select>
          )}
        </div>
      )}
      <div className="h-80 overflow-y-auto rounded-md border border-border bg-muted/30 p-2 font-mono text-sm">
        {log.map((e) => (
          <div key={e.id} className={`flex justify-between gap-4 rounded px-1 ${e.stage === "running" ? "bg-accent" : ""}`}>
            <span className={e.status === "print" ? "text-muted-foreground" : ""}>{e.status === "print" ? e.line : `> ${e.line}`}</span>
            {e.status !== "print" && <span className={`shrink-0 ${statusClass(e)}`}>{statusText(e)}</span>}
          </div>
        ))}
        <div ref={logEnd} />
      </div>
      <div className="mt-2 flex gap-2">
        <TextInput
          autoFocus
          className="w-full font-mono"
          value={input}
          onChange={(e) => {
            setInput(e.target.value)
          }}
          onKeyDown={onKeyDown}
          onPaste={onPaste}
          placeholder="G-code (LinuxCNC) or a command, e.g. G91 G0 Z-1"
          spellCheck={false}
          autoCapitalize="off"
          autoComplete="off"
        />
        <Button variant="danger" onClick={() => queue.sendNow("stop")}>
          Stop
        </Button>
      </div>
      <p className="mt-1 text-xs text-muted-foreground">Enter sends. Up and Down recall earlier lines. Esc clears the line. Pasted lines run one by one; an error skips the rest.</p>
    </Panel>
  )
}
