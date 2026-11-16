"use client"

import { useEffect, useRef, useState, type KeyboardEvent } from "react"
import { sendCommand } from "../api"
import { addToHistory, freshRecall, loadHistory, saveHistory, stepHistory, type Recall } from "../mdi-history"
import { useDeviceState } from "../state-provider"
import type { CmdOutcome } from "../types"
import { Button, Panel, Select, TextInput } from "../ui/primitives"

interface Entry {
  id: number
  line: string
  reply: string | null
  ok: boolean
}

function replyText(outcome: CmdOutcome): { reply: string; ok: boolean } {
  if (outcome.status === "denied") {
    return { reply: `err ${outcome.msg}`, ok: false }
  }
  if (outcome.status === "unconfirmed") {
    return { reply: "no reply", ok: false }
  }
  const msg = (outcome.data as { msg?: unknown } | undefined)?.msg
  return { reply: typeof msg === "string" && msg ? `ok ${msg}` : "ok", ok: true }
}

const WCS_WORDS = ["G54", "G55", "G56", "G57", "G58", "G59"]

export function MdiScreen() {
  const { state, capabilities } = useDeviceState()
  const [history, setHistory] = useState<string[]>(() => loadHistory())
  const [recall, setRecall] = useState<Recall>(() => freshRecall(history))
  const [input, setInput] = useState("")
  const [log, setLog] = useState<Entry[]>([])
  const nextId = useRef(0)
  const logEnd = useRef<HTMLDivElement>(null)
  const homed = !!state && state.z.homed && (!capabilities?.has_x_motor || state.x.homed)

  useEffect(() => {
    logEnd.current?.scrollIntoView({ block: "nearest" })
  }, [log])

  async function run(id: number, line: string) {
    const { reply, ok } = replyText(await sendCommand(line))
    setLog((l) => l.map((e) => (e.id === id ? { ...e, reply, ok } : e)))
  }

  function send(line: string) {
    const id = nextId.current++
    setLog((l) => [...l, { id, line, reply: null, ok: true }])
    void run(id, line)
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
    send(line)
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
          <Button onClick={() => setLog([])} disabled={!log.length}>
            Clear
          </Button>
          <Button variant="danger" onClick={() => send("stop")}>
            Stop
          </Button>
        </div>
      }
    >
      {state && (
        <div className="mb-2 flex flex-wrap items-center gap-2">
          {state.state === "estop" ? (
            <Button variant="danger" onClick={() => send("estop off")}>
              EStop on
            </Button>
          ) : (
            <Button onClick={() => send("estop on")}>EStop off</Button>
          )}
          <Button variant={state.z.enabled ? "primary" : "secondary"} onClick={() => send(`power ${state.z.enabled ? "off" : "on"} z`)}>
            Z motor {state.z.enabled ? "on" : "off"}
          </Button>
          {capabilities?.has_x_motor && (
            <Button variant={state.x.enabled ? "primary" : "secondary"} onClick={() => send(`power ${state.x.enabled ? "off" : "on"} x`)}>
              X motor {state.x.enabled ? "on" : "off"}
            </Button>
          )}
          {capabilities?.has_endstops && (
            <Button
              variant={homed ? "primary" : "secondary"}
              disabled={state.homing}
              onClick={() => send(homed ? "unhome" : "home")}
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
                send(e.target.value)
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
          <div key={e.id}>
            <div>&gt; {e.line}</div>
            {e.reply !== null && <div className={e.ok ? "text-muted-foreground" : "text-destructive"}>{e.reply}</div>}
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
          placeholder="G-code (LinuxCNC) or a command, e.g. G91 G0 Z-1"
          spellCheck={false}
          autoCapitalize="off"
          autoComplete="off"
        />
        <Button variant="danger" onClick={() => send("stop")}>
          Stop
        </Button>
      </div>
      <p className="mt-1 text-xs text-muted-foreground">Enter sends. Up and Down recall earlier lines. Esc clears the line.</p>
    </Panel>
  )
}
