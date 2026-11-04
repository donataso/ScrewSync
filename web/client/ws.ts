import type { CmdOutcome, ScanResult, StatusPush, WifiPush } from "./types"

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
  onScan?: (s: ScanResult) => void
  onWifi?: (s: WifiPush) => void
  onConnected?: (connected: boolean, error: string | null) => void
}

let socket: WebSocket | null = null
let uploading = false // true for the whole duration of an /update upload -- no reconnect attempts at all
let reloadOnReconnect = false // true only once that upload has actually finished successfully
let reconnectDelay = RECONNECT_MIN_MS
let reconnectTimer: ReturnType<typeof setTimeout> | null = null
let staleTimer: ReturnType<typeof setInterval> | null = null
let lastRecvMs = 0
let pending: { expectedType: string; resolve: (o: CmdOutcome) => void; timer: ReturnType<typeof setTimeout> } | null = null
const listeners = new Set<Listener>()

function notifyConnected(connected: boolean, error: string | null) {
  for (const l of listeners) l.onConnected?.(connected, error)
}

function resolvePending(outcome: CmdOutcome) {
  if (!pending) return
  clearTimeout(pending.timer)
  pending.resolve(outcome)
  pending = null
}

// Pushes also fire unsolicited (periodic telemetry, on connect) -- so the
// next message isn't necessarily this command's reply. Only resolve pending
// on the push type this command's domain actually changes.
function expectedReplyType(line: string): string {
  const verb = line.split(" ", 1)[0]
  if (verb === "settings") return "settings"
  if (verb === "display") return "display"
  if (verb === "wifi") return line.startsWith("wifi scan") ? "scan" : "wifi"
  return "status"
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
      if (Date.now() - lastRecvMs > STALE_MS) ws.close()
    }, STALE_MS)
  }

  ws.onmessage = (ev) => {
    lastRecvMs = Date.now()
    let msg: AnyMsg
    try {
      msg = JSON.parse(ev.data)
    } catch {
      return
    }

    // The firmware has no reply envelope/id (docs/protocol.md): a command's
    // "reply" is the typed push its own domain causes. But pushes (status
    // especially) also fire unsolicited/periodically, so only a push of the
    // type this command actually expects may resolve it -- anything else
    // just goes to listeners below.
    if (msg.type === "error") {
      resolvePending({ status: "denied", msg: (msg as ErrorMsg).msg ?? "" })
    } else if (pending && msg.type === pending.expectedType) {
      const { type, ...data } = msg
      resolvePending({ status: "ok", msg: "", data })
    }

    if (msg.type === "status") {
      const { type, ...status } = msg
      for (const l of listeners) l.onStatus?.(status as StatusPush)
    } else if (msg.type === "scan") {
      const { type, ...scan } = msg
      for (const l of listeners) l.onScan?.(scan as ScanResult)
    } else if (msg.type === "wifi") {
      const { type, ...wifi } = msg
      for (const l of listeners) l.onWifi?.(wifi as WifiPush)
    }
  }

  ws.onclose = (ev) => {
    if (staleTimer) clearInterval(staleTimer)
    resolvePending({ status: "unconfirmed" })
    const full = ev.code === SERVER_FULL_CLOSE_CODE
    notifyConnected(false, full ? "Server has reached its client limit." : null)
    if (full) reconnectDelay = Math.max(reconnectDelay, FULL_RETRY_MS)
    scheduleReconnect()
  }

  ws.onerror = () => ws.close()
}

function scheduleReconnect() {
  if (uploading) return // suspended entirely -- nothing to connect to mid-upload
  if (reconnectTimer) return
  const delay = reloadOnReconnect ? FLASHING_RETRY_MS : reconnectDelay
  reconnectTimer = setTimeout(() => {
    reconnectTimer = null
    connect()
  }, delay)
  if (!reloadOnReconnect) reconnectDelay = Math.min(reconnectDelay * 2, RECONNECT_MAX_MS)
}

function sendRaw(line: string) {
  if (socket?.readyState === WebSocket.OPEN) socket.send(JSON.stringify({ type: "cmd", line }))
}

export function wsConnect() {
  if (!socket) connect()
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
    resolvePending({ status: "unconfirmed" })
    pending = {
      expectedType: expectedReplyType(line),
      resolve,
      timer: setTimeout(() => resolvePending({ status: "unconfirmed" }), CMD_TIMEOUT_MS),
    }
    sendRaw(line)
  })
}
