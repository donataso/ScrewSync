import { parseProbe, type ProbeResult } from "./probe"
import type { Capabilities, CmdOutcome, InputsPush, ScanResult, StatusPush, WifiPush } from "./types"

interface StatusMsg extends StatusPush {
  type: "status"
}
interface ScanMsg extends ScanResult {
  type: "scan"
}
interface WifiMsg extends WifiPush {
  type: "wifi"
}
interface ErrorMsg {
  type: "error"
  msg: string
}
// settings/commands carry fields this client doesn't model yet -- handled
// generically below just to resolve a pending command's promise.
interface OtherMsg {
  type: string
  [key: string]: unknown
}

type AnyMsg = StatusMsg | ScanMsg | WifiMsg | ErrorMsg | OtherMsg

const RECONNECT_MIN_MS = 500
const RECONNECT_MAX_MS = 1000
const FULL_RETRY_MS = 2000
const FLASHING_RETRY_MS = 5000
const STALE_MS = 3000
const CMD_TIMEOUT_MS = 3000
const SERVER_FULL_CLOSE_CODE = 1013

type Listener = {
  onStatus?: (s: StatusPush) => void
  onCapabilities?: (c: Capabilities) => void
  onScan?: (s: ScanResult) => void
  onInputs?: (s: InputsPush) => void
  onWifi?: (s: WifiPush) => void
  onProbe?: (r: ProbeResult) => void
  // A G-code (PRINT, ...) line.
  onPrint?: (text: string) => void
  onConnected?: (connected: boolean, error: string | null) => void
}

let socket: WebSocket | null = null
let uploading = false // true for the whole duration of an /update upload -- no reconnect attempts at all
let reloadOnReconnect = false // true only once that upload has actually finished successfully
let reconnectDelay = RECONNECT_MIN_MS
let reconnectTimer: ReturnType<typeof setTimeout> | null = null
let staleTimer: ReturnType<typeof setInterval> | null = null
let lastRecvMs = 0
let nextId = 1
const pending = new Map<number, { resolve: (o: CmdOutcome) => void; timer: ReturnType<typeof setTimeout> }>()
const listeners = new Set<Listener>()

function notifyConnected(connected: boolean, error: string | null) {
  for (const l of listeners) {
l.onConnected?.(connected, error)
}
}

function resolvePending(id: number, outcome: CmdOutcome) {
  const p = pending.get(id)
  if (!p) {
    return
  }
  clearTimeout(p.timer)
  pending.delete(id)
  p.resolve(outcome)
}

function resolveAllPending(outcome: CmdOutcome) {
  for (const id of [...pending.keys()]) {
    resolvePending(id, outcome)
  }
}

function wsUrl(): string {
  const proto = window.location.protocol === "https:" ? "wss:" : "ws:"
  return `${proto}//${window.location.host}/ws`
}

function connect() {
  const ws = new WebSocket(wsUrl())
  socket = ws

  ws.onopen = () => {
    // The upload finished and the device rebooted into whatever was
    // flashed -- resuming the old page against new firmware is the wrong
    // move, reload instead. Only armed once the upload itself reported
    // success (see wsNotifyFlashed), never while it's still in flight --
    // a stray reconnect mid-upload must not reload and cancel it.
    if (reloadOnReconnect) {
      window.location.reload()
      return
    }
    reconnectDelay = RECONNECT_MIN_MS
    notifyConnected(true, null)
    lastRecvMs = Date.now()
    staleTimer = setInterval(() => {
      if (Date.now() - lastRecvMs > STALE_MS) {
ws.close()
}
    }, STALE_MS)
  }

  ws.onmessage = (ev) => {
    lastRecvMs = Date.now()
    let parsed: AnyMsg & { id?: unknown }
    try {
      parsed = JSON.parse(ev.data)
    } catch {
      return
    }
    const { id, ...rest } = parsed
    const msg = rest as AnyMsg

    if (typeof id === "number") {
      if (msg.type === "error") {
        resolvePending(id, { status: "denied", msg: (msg as ErrorMsg).msg ?? "" })
      } else {
        const { type, ...data } = msg
        resolvePending(id, { status: "ok", msg: type === "reply" ? String((data as { msg?: unknown }).msg ?? "") : "", data })
      }
    }

    if (msg.type === "status") {
      const { type, ...status } = msg
      for (const l of listeners) {
l.onStatus?.(status as StatusPush)
}
    } else if (msg.type === "capabilities") {
      const { type, ...capabilities } = msg
      for (const l of listeners) {
l.onCapabilities?.(capabilities as unknown as Capabilities)
}
    } else if (msg.type === "inputs") {
      const { type, ...inputs } = msg
      for (const l of listeners) {
l.onInputs?.(inputs as unknown as InputsPush)
}
    } else if (msg.type === "scan") {
      const { type, ...scan } = msg
      for (const l of listeners) {
l.onScan?.(scan as ScanResult)
}
    } else if (msg.type === "wifi") {
      const { type, ...wifi } = msg
      for (const l of listeners) {
l.onWifi?.(wifi as WifiPush)
}
    } else if (msg.type === "probe") {
      const result = parseProbe(String((msg as OtherMsg).msg ?? ""))
      if (result) {
        for (const l of listeners) {
l.onProbe?.(result)
}
      }
    } else if (msg.type === "print") {
      const text = String((msg as OtherMsg).msg ?? "")
      for (const l of listeners) {
l.onPrint?.(text)
}
    }
  }

  ws.onclose = (ev) => {
    if (staleTimer) {
clearInterval(staleTimer)
}
    resolveAllPending({ status: "unconfirmed" })
    const full = ev.code === SERVER_FULL_CLOSE_CODE
    notifyConnected(false, full ? "Server has reached its client limit." : null)
    if (full) {
reconnectDelay = Math.max(reconnectDelay, FULL_RETRY_MS)
}
    scheduleReconnect()
  }

  ws.onerror = () => ws.close()
}

function scheduleReconnect() {
  if (uploading) {
return
} // suspended entirely -- nothing to connect to mid-upload
  if (reconnectTimer) {
return
}
  const delay = reloadOnReconnect ? FLASHING_RETRY_MS : reconnectDelay
  reconnectTimer = setTimeout(() => {
    reconnectTimer = null
    connect()
  }, delay)
  if (!reloadOnReconnect) {
reconnectDelay = Math.min(reconnectDelay * 2, RECONNECT_MAX_MS)
}
}

function sendRaw(line: string, id?: number) {
  if (socket?.readyState === WebSocket.OPEN) {
    socket.send(JSON.stringify({ type: "cmd", line, id }))
  }
}

export function wsInputs(on: boolean) {
  sendRaw(on ? "inputs on" : "inputs off")
}

export function wsConnect() {
  if (!socket) {
connect()
}
}

// Call when a firmware upload starts/ends: suspends reconnect attempts
// entirely for the duration -- the device can't accept a WS connection
// mid-upload anyway, so there's nothing to retry towards. Ending it with
// `active: false` (upload rejected/failed, never actually flashed) resumes
// normal fast reconnect immediately.
export function wsNotifyFlashing(active: boolean) {
  uploading = active
  if (!active) {
    reloadOnReconnect = false
    scheduleReconnect()
  }
}

// Call once the upload has reported success: the device is rebooting, so
// reconnect attempts resume (at FLASHING_RETRY_MS, waiting for it to come
// back) and the next successful one means new firmware is up -- reload then.
export function wsNotifyFlashed() {
  uploading = false
  reloadOnReconnect = true
  scheduleReconnect()
}

export function wsSubscribe(l: Listener): () => void {
  listeners.add(l)
  return () => listeners.delete(l)
}

export function wsSend(line: string): Promise<CmdOutcome> {
  return new Promise((resolve) => {
    if (socket?.readyState !== WebSocket.OPEN) {
      resolve({ status: "unconfirmed" })
      return
    }
    const id = nextId++
    pending.set(id, { resolve, timer: setTimeout(() => resolvePending(id, { status: "unconfirmed" }), CMD_TIMEOUT_MS) })
    sendRaw(line, id)
  })
}
