"use client"

import { useEffect, useRef, useState } from "react"
import { downloadBackup, fetchDeviceLog, sendCommand, uploadBackup, uploadDisplay, uploadFirmware } from "../api"
import { cmd } from "../commands"
import { CONFIGURE_LOCK_MESSAGE, useCanConfigure } from "../state-provider"
import { Button, LockNotice, Panel, Toggle } from "../ui/primitives"
import { useToast } from "../ui/toast"

interface StoredDisplay {
  length: number
  crc: string
}

// "stored=1 length=<n> crc=<hex>" | "stored=0"
function parseDisplayInfo(msg: string): StoredDisplay | null {
  const kv = Object.fromEntries(msg.split(" ").map((p) => p.split("=") as [string, string]))
  return kv.stored === "1" ? { length: Number(kv.length), crc: kv.crc } : null
}

export function DeviceScreen() {
  const [busy, setBusy] = useState(false)
  const toast = useToast()
  const [progress, setProgress] = useState<number | null>(null)
  const [firmwareFile, setFirmwareFile] = useState<File | null>(null)
  const [deviceBackupFile, setDeviceBackupFile] = useState<File | null>(null)
  const deviceBackupInputRef = useRef<HTMLInputElement>(null)
  const firmwareInputRef = useRef<HTMLInputElement>(null)
  const [logLines, setLogLines] = useState<string[]>([])
  const [logBusy, setLogBusy] = useState(false)

  const canUpload = useCanConfigure()
  const configureOk = canUpload
  const [displayFile, setDisplayFile] = useState<File | null>(null)
  const [displayStore, setDisplayStore] = useState(true)
  const [displayProgress, setDisplayProgress] = useState<number | null>(null)
  const [storedDisplay, setStoredDisplay] = useState<StoredDisplay | null | undefined>(undefined)
  const displayInputRef = useRef<HTMLInputElement>(null)

  async function loadDisplayInfo() {
    const outcome = await sendCommand(cmd.displayInfo())
    if (outcome.status === "ok") {
setStoredDisplay(parseDisplayInfo(String((outcome.data as { msg?: string })?.msg ?? "")))
}
  }

  useEffect(() => {
    loadDisplayInfo()
  }, [])

  async function uploadDisplayFile() {
    if (!displayFile) {
return
}
    setBusy(true)
    setDisplayProgress(0)
    try {
      const result = await uploadDisplay(displayFile, displayStore, setDisplayProgress)
      if (result.ok) {
toast.push(`Display flashed: ${result.message}`, "good")
} else {
toast.push(`Display update failed: ${result.message}`, "bad")
}
    } finally {
      setBusy(false)
      setDisplayProgress(null)
      setDisplayFile(null)
      if (displayInputRef.current) {
displayInputRef.current.value = ""
}
    }
    loadDisplayInfo()
  }

  async function flashStoredDisplay() {
    if (!confirm("Flash the stored display firmware to the display now?")) {
return
}
    setBusy(true)
    try {
      const outcome = await sendCommand(cmd.displayFlash())
      if (outcome.status === "ok") {
toast.push("Flashing the display from the stored image.", "info")
} else {
toast.push(outcome.status === "denied" ? `Denied: ${outcome.msg}` : "No reply from device.", "bad")
}
    } finally {
      setBusy(false)
    }
  }

  async function exportDeviceBackup() {
    setBusy(true)
    try {
      const url = URL.createObjectURL(await downloadBackup())
      const a = document.createElement("a")
      a.href = url
      a.download = "screwsync-device-backup.json"
      a.click()
      URL.revokeObjectURL(url)
    } catch (e) {
      toast.push(`Export failed: ${e instanceof Error ? e.message : String(e)}`, "bad")
    } finally {
      setBusy(false)
    }
  }

  async function importDeviceBackup() {
    if (!deviceBackupFile) {
return
}
    setBusy(true)
    try {
      const result = await uploadBackup(deviceBackupFile)
      if (result.ok) {
        toast.push("Imported. Rebooting.", "good")
        await sendCommand(cmd.reboot())
      } else {
toast.push(`Import rejected: ${result.message}`, "bad")
}
    } catch (e) {
      toast.push(`Import failed: ${e instanceof Error ? e.message : String(e)}`, "bad")
    } finally {
      setBusy(false)
      setDeviceBackupFile(null)
      if (deviceBackupInputRef.current) {
deviceBackupInputRef.current.value = ""
}
    }
  }

  async function uploadFirmwareFile() {
    if (!firmwareFile) {
return
}
    setBusy(true)
    setProgress(0)
    try {
      const result = await uploadFirmware(firmwareFile, setProgress)
      if (result.ok) {
toast.push("Update accepted - device is rebooting. Do not close this tab or power off for about 10 seconds.", "good")
} else {
toast.push(`Update rejected: ${result.message}`, "bad")
}
    } finally {
      setBusy(false)
      setProgress(null)
      setFirmwareFile(null)
      if (firmwareInputRef.current) {
firmwareInputRef.current.value = ""
}
    }
  }

  async function fetchLog() {
    setLogBusy(true)
    try {
      const lines = await fetchDeviceLog()
      setLogLines(lines)
      if (!lines.length) {
toast.push("Log is empty.", "info")
} else {
toast.push(`Fetched ${lines.length} line(s).`, "good")
}
    } catch (e) {
      toast.push(`Fetch failed: ${e instanceof Error ? e.message : String(e)}`, "bad")
    } finally {
      setLogBusy(false)
    }
  }

  async function reboot() {
    if (!confirm("Reboot the device now?")) {
return
}
    setBusy(true)
    try {
      const outcome = await sendCommand(cmd.reboot())
      if (outcome.status === "denied") {
toast.push(`Denied: ${outcome.msg || "rejected by device"}`, "bad")
} else {
toast.push("Reboot requested.", "info")
}
    } finally {
      setBusy(false)
    }
  }

  return (
    <div className="flex flex-col gap-4">
      <h1 className="text-lg font-semibold">Device</h1>

      <Panel title="Device backup">
        {!configureOk && <LockNotice>{CONFIGURE_LOCK_MESSAGE}</LockNotice>}
        <div className="flex flex-wrap items-center gap-2">
          <Button disabled={busy || !configureOk} onClick={exportDeviceBackup}>
            Export
          </Button>
          <Button disabled={busy || !configureOk} onClick={() => deviceBackupInputRef.current?.click()}>
            Choose file
          </Button>
          <input
            ref={deviceBackupInputRef}
            type="file"
            accept="application/json"
            className="hidden"
            disabled={busy || !configureOk}
            onChange={(e) => setDeviceBackupFile(e.target.files?.[0] ?? null)}
          />
          {deviceBackupFile && <span className="text-sm text-muted-foreground">{deviceBackupFile.name}</span>}
          <Button variant="primary" disabled={busy || !configureOk || !deviceBackupFile} onClick={importDeviceBackup}>
            Import and reboot
          </Button>
        </div>
      </Panel>

      <Panel title="Device log">
        {!configureOk && <LockNotice>{CONFIGURE_LOCK_MESSAGE}</LockNotice>}
        <div className="mb-3 flex items-center gap-3">
          <Button disabled={logBusy || !configureOk} onClick={fetchLog}>
            Fetch log
          </Button>
        </div>
        {logLines.length > 0 && (
          <pre className="max-h-80 overflow-y-auto rounded-md bg-muted p-2 text-xs text-muted-foreground">
            {logLines.join("\n")}
          </pre>
        )}
      </Panel>

      <Panel title="Firmware update">
        {!canUpload && <LockNotice>{CONFIGURE_LOCK_MESSAGE}</LockNotice>}
        <div className="flex flex-col gap-2">
          <div className="flex flex-wrap items-center gap-2">
            <Button disabled={!canUpload || busy} onClick={() => firmwareInputRef.current?.click()}>
              Choose file
            </Button>
            <input
              ref={firmwareInputRef}
              type="file"
              className="hidden"
              disabled={!canUpload || busy}
              onChange={(e) => setFirmwareFile(e.target.files?.[0] ?? null)}
            />
            {firmwareFile && <span className="text-sm text-muted-foreground">{firmwareFile.name}</span>}
            <Button variant="primary" disabled={!canUpload || busy || !firmwareFile} onClick={uploadFirmwareFile}>
              Upload
            </Button>
          </div>
          {progress !== null && (
            <div className="h-2 w-full overflow-hidden rounded-full bg-muted">
              <div className="h-full bg-primary transition-all" style={{ width: `${progress}%` }} />
            </div>
          )}
        </div>
      </Panel>

      <Panel title="Display firmware">
        {!canUpload && <LockNotice>{CONFIGURE_LOCK_MESSAGE}</LockNotice>}
        <div className="flex flex-col gap-3">
          <div className="flex flex-wrap items-center gap-2">
            <span className="text-sm text-muted-foreground">
              {storedDisplay === undefined
                ? "Stored: unknown"
                : storedDisplay
                  ? `Stored: ${(storedDisplay.length / 1024).toFixed(0)} KiB, CRC ${storedDisplay.crc}`
                  : "Nothing stored"}
            </span>
            <Button disabled={!canUpload || busy || !storedDisplay} onClick={flashStoredDisplay}>
              Flash stored display firmware
            </Button>
          </div>
          <div className="flex flex-wrap items-center gap-2">
            <Button disabled={!canUpload || busy} onClick={() => displayInputRef.current?.click()}>
              Choose file
            </Button>
            <input
              ref={displayInputRef}
              type="file"
              accept=".tft"
              className="hidden"
              disabled={!canUpload || busy}
              onChange={(e) => setDisplayFile(e.target.files?.[0] ?? null)}
            />
            {displayFile && <span className="text-sm text-muted-foreground">{displayFile.name}</span>}
            <label className="flex items-center gap-2 text-sm text-muted-foreground">
              <Toggle checked={displayStore} onChange={setDisplayStore} disabled={!canUpload || busy} label="Store on device" />
              Store on device
            </label>
            <Button variant="primary" disabled={!canUpload || busy || !displayFile} onClick={uploadDisplayFile}>
              Upload
            </Button>
          </div>
          {displayProgress !== null && (
            <div className="h-2 w-full overflow-hidden rounded-full bg-muted">
              <div className="h-full bg-primary transition-all" style={{ width: `${displayProgress}%` }} />
            </div>
          )}
        </div>
      </Panel>

      <Panel title="Reboot">
        {!configureOk && <LockNotice>{CONFIGURE_LOCK_MESSAGE}</LockNotice>}
        <Button variant="danger" disabled={busy || !configureOk} onClick={reboot}>
          Reboot device
        </Button>
      </Panel>
    </div>
  )
}
