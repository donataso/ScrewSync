// HTTP helpers for the /gcode* and /display-preview endpoints
// (main/lib/WebUi/WebGcode.cpp, main/lib/GcodeStore/GcodeStore.hpp).
import type { Mapping } from "./preview"

export interface GcodeProgramInfo {
  id: string
  name: string
  flavor: string
  size: number
}

export interface GcodeActiveInfo {
  name: string
  flavor: string
  size: number
  steps: number
  id: string | null
  version: number
}

export interface GcodeListResult {
  programs: GcodeProgramInfo[]
  free: number
  active: GcodeActiveInfo | null
}

export interface GcodeApiError {
  ok: false
  error: string
  line?: number
}

export type GcodeUploadResult = { ok: true; id: string | null; active: GcodeActiveInfo | null } | GcodeApiError

async function readError(res: Response): Promise<string> {
  const text = (await res.text().catch(() => "")).trim()
  try {
    const parsed = JSON.parse(text) as GcodeApiError
    if (parsed && parsed.ok === false && parsed.error) {
      return parsed.line ? `${parsed.error} (line ${parsed.line})` : parsed.error
    }
  } catch {
    // not JSON, e.g. the 403/409 plaintext replies
  }
  return text || `HTTP ${res.status}`
}

export async function fetchGcodeList(): Promise<GcodeListResult> {
  const res = await fetch("/gcode")
  if (!res.ok) throw new Error(await readError(res))
  return res.json()
}

export async function fetchGcodeActiveText(): Promise<string | null> {
  const res = await fetch("/gcode/active")
  if (res.status === 404) return null
  if (!res.ok) throw new Error(await readError(res))
  return res.text()
}

export async function fetchGcodeProgramText(id: string): Promise<string> {
  const res = await fetch(`/gcode/program?id=${encodeURIComponent(id)}`)
  if (!res.ok) throw new Error(await readError(res))
  return res.text()
}

// "save" stores it without running it; "run" makes it the lathe's program without saving.
export async function uploadGcode(
  text: string,
  name: string,
  flavor: string,
  target: "save" | "run",
): Promise<GcodeUploadResult> {
  const res = await fetch(`/gcode/${target}`, {
    method: "POST",
    headers: { "Content-Type": "text/plain" },
    body: `${flavor}\t${name}\n${text}`,
  })
  if (!res.ok) return { ok: false, error: await readError(res) }
  return res.json()
}

// Only the fields given are changed; omitted ones are sent empty.
export async function updateGcode(
  id: string,
  changes: { name?: string; flavor?: string; text?: string },
): Promise<GcodeUploadResult> {
  const res = await fetch(`/gcode/update?id=${encodeURIComponent(id)}`, {
    method: "POST",
    headers: { "Content-Type": "text/plain" },
    body: `${changes.flavor ?? ""}\t${changes.name ?? ""}\n${changes.text ?? ""}`,
  })
  if (!res.ok) return { ok: false, error: await readError(res) }
  return res.json()
}

export async function selectGcode(id: string): Promise<GcodeUploadResult> {
  const res = await fetch(`/gcode/select?id=${encodeURIComponent(id)}`, { method: "POST" })
  if (!res.ok) return { ok: false, error: await readError(res) }
  return res.json()
}

export async function deleteGcode(id: string): Promise<{ ok: boolean; message: string }> {
  const res = await fetch(`/gcode/delete?id=${encodeURIComponent(id)}`, { method: "POST" })
  const message = await readError(res).catch(() => "")
  return { ok: res.ok, message: res.ok ? "" : message }
}

export async function uploadDisplayPreview(
  blob: Blob,
  mapping: Pick<Mapping, "originX" | "originY" | "pxPerMm">,
): Promise<{ ok: boolean; message: string }> {
  const query = `ox=${mapping.originX.toFixed(2)}&oy=${mapping.originY.toFixed(2)}&s=${mapping.pxPerMm.toFixed(4)}`
  const res = await fetch(`/display-preview?${query}`, {
    method: "POST",
    headers: { "Content-Type": "image/jpeg" },
    body: blob,
  })
  const message = (await res.text().catch(() => "")).trim()
  return { ok: res.ok, message }
}
