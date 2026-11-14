import type { CmdOutcome } from "./types"
import type { PinFunctionsJson, PinsJson, PinsUpdateJson } from "./pins"
import type { ThreadTableJson } from "./threads"
import { wsSend, wsNotifyFlashing, wsNotifyFlashed } from "./ws"

export async function sendCommand(line: string): Promise<CmdOutcome> {
  return wsSend(line)
}

export async function fetchDeviceLog(): Promise<string[]> {
  const res = await fetch("/log")
  if (!res.ok) {
throw new Error((await res.text().catch(() => "")).trim() || `HTTP ${res.status}`)
}
  return res.json()
}

export async function joinWifi(ssid: string, password: string): Promise<{ ok: boolean; message: string }> {
  const body = new URLSearchParams({ s: ssid, p: password })
  const res = await fetch("/wifi", { method: "POST", body })
  const message = await res.text().catch(() => "")
  return { ok: res.ok, message }
}

export async function uploadFirmware(
  file: File,
  onProgress?: (pct: number) => void,
): Promise<{ ok: boolean; message: string }> {
  return new Promise((resolve) => {
    const xhr = new XMLHttpRequest()
    // Raw body, not FormData/multipart -- firmware's /update handler writes
    // the POST body straight into flash as the image, same as `curl --data-binary`.
    xhr.upload.onprogress = (e) => {
      if (e.lengthComputable && onProgress) {
onProgress(Math.round((e.loaded / e.total) * 100))
}
    }
    xhr.onload = () => {
      const ok = xhr.status >= 200 && xhr.status < 300
      // Only now do we know the device is actually rebooting -- arms the
      // reload-on-reconnect. A rejected/failed upload just goes back to
      // normal reconnect behavior, nothing to wait for.
      if (ok) {
wsNotifyFlashed()
} else {
wsNotifyFlashing(false)
}
      resolve({ ok, message: xhr.responseText })
    }
    xhr.onerror = () => {
      wsNotifyFlashing(false)
      resolve({ ok: false, message: "network error" })
    }
    wsNotifyFlashing(true)
    xhr.open("POST", "/update")
    xhr.setRequestHeader("Content-Type", "application/octet-stream")
    xhr.send(file)
  })
}

export async function uploadDisplay(
  file: File,
  store: boolean,
  onProgress?: (pct: number) => void,
): Promise<{ ok: boolean; message: string }> {
  return new Promise((resolve) => {
    const xhr = new XMLHttpRequest()
    xhr.upload.onprogress = (e) => {
      if (e.lengthComputable && onProgress) {
onProgress(Math.round((e.loaded / e.total) * 100))
}
    }
    const done = (ok: boolean, message: string) => {
      wsNotifyFlashing(false)
      resolve({ ok, message })
    }
    xhr.onload = () => done(xhr.status >= 200 && xhr.status < 300, xhr.responseText.trim())
    xhr.onerror = () => done(false, "network error")
    wsNotifyFlashing(true)
    xhr.open("POST", "/update-display")
    xhr.setRequestHeader("Content-Type", "application/octet-stream")
    if (store) {
xhr.setRequestHeader("X-Display-Store", "1")
}
    xhr.send(file)
  })
}

export async function downloadBackup(): Promise<Blob> {
  const res = await fetch("/backup")
  if (!res.ok) {
throw new Error((await res.text().catch(() => "")).trim() || `HTTP ${res.status}`)
}
  return res.blob()
}

export async function uploadBackup(file: File): Promise<{ ok: boolean; message: string }> {
  const res = await fetch("/backup", { method: "POST", headers: { "Content-Type": "application/json" }, body: file })
  const message = (await res.text().catch(() => "")).trim()
  return { ok: res.ok, message }
}

export async function fetchThreads(): Promise<ThreadTableJson> {
  const res = await fetch("/threads")
  if (!res.ok) {
throw new Error((await res.text().catch(() => "")).trim() || `HTTP ${res.status}`)
}
  return res.json()
}

export async function saveThreads(table: ThreadTableJson): Promise<{ ok: boolean; message: string }> {
  const res = await fetch("/threads", { method: "POST", headers: { "Content-Type": "application/json" }, body: JSON.stringify(table) })
  const message = (await res.text().catch(() => "")).trim()
  return { ok: res.ok, message }
}

export async function fetchPins(): Promise<PinsJson> {
  const res = await fetch("/pins")
  if (!res.ok) {
    throw new Error((await res.text().catch(() => "")).trim() || `HTTP ${res.status}`)
  }
  return res.json()
}

export async function fetchPinFunctions(): Promise<PinFunctionsJson> {
  const res = await fetch("/pins/functions")
  if (!res.ok) {
    throw new Error((await res.text().catch(() => "")).trim() || `HTTP ${res.status}`)
  }
  return res.json()
}

export async function savePins(update: PinsUpdateJson): Promise<{ ok: boolean; message: string }> {
  const res = await fetch("/pins", { method: "POST", headers: { "Content-Type": "application/json" }, body: JSON.stringify(update) })
  const message = (await res.text().catch(() => "")).trim()
  return { ok: res.ok, message }
}
