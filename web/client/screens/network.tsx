"use client"

import { useEffect, useState } from "react"
import { joinWifi, sendCommand } from "../api"
import { cmd } from "../commands"
import type { ScanResult, WifiPush } from "../types"
import { wsSubscribe } from "../ws"
import { Badge, Button, Field, Panel, TextInput } from "../ui/primitives"
import { useToast } from "../ui/toast"

export function NetworkScreen() {
  const [status, setStatus] = useState<WifiPush | null>(null)
  const [scan, setScan] = useState<ScanResult | null>(null)
  const [scanning, setScanning] = useState(false)
  const [ssid, setSsid] = useState("")
  const [password, setPassword] = useState("")
  const [joining, setJoining] = useState(false)
  const [connecting, setConnecting] = useState(false)
  const toast = useToast()

  async function reload() {
    const s = await sendCommand("wifi status")
    if (s.status === "ok") {
setStatus(s.data as WifiPush)
}
  }

  useEffect(() => {
    reload()
    return wsSubscribe({
      onWifi: setStatus,
      onScan: (s) => {
        setScan(s)
        setScanning(false)
      },
    })
  }, [])

  async function startScan() {
    setScanning(true)
    await sendCommand(cmd.wifiScan())
  }

  async function connectSaved(ssid: string) {
    setConnecting(true)
    try {
      const outcome = await sendCommand(cmd.wifiUse(ssid))
      if (outcome.status === "denied") {
toast.push(`Denied: ${outcome.msg || "rejected by device"}`, "bad")
} else {
toast.push(`Connecting to "${ssid}"...`, "good")
}
      await reload()
    } finally {
      setConnecting(false)
    }
  }

  async function removeSaved(ssid: string) {
    if (!confirm(`Remove saved network "${ssid}"?`)) {
return
}
    setConnecting(true)
    try {
      const outcome = await sendCommand(cmd.wifiDel(ssid))
      if (outcome.status === "denied") {
toast.push(`Denied: ${outcome.msg || "rejected by device"}`, "bad")
} else {
toast.push(`Removed "${ssid}".`, "good")
}
      await reload()
    } finally {
      setConnecting(false)
    }
  }

  async function join() {
    setJoining(true)
    try {
      const result = await joinWifi(ssid, password)
      if (result.ok) {
toast.push("Joining network...", "good")
} else {
toast.push(`Denied: ${result.message || "rejected"}`, "bad")
}
      await reload()
    } finally {
      setJoining(false)
    }
  }

  if (!status) {
    return (
      <Panel title="Network">
        <p className="text-sm text-muted-foreground">Loading...</p>
      </Panel>
    )
  }

  return (
    <div className="flex flex-col gap-4">
      <h1 className="text-lg font-semibold">Network</h1>
      <Panel
        title="Status"
        action={<Badge tone={status.mode === "station" ? "good" : status.mode === "ap" ? "info" : "warn"}>{status.mode}</Badge>}
      >
        <div className="grid gap-2 text-sm md:grid-cols-3">
          <div>
            <div className="text-muted-foreground text-xs">SSID</div>
            <div>{status.ssid || "-"}</div>
          </div>
          <div>
            <div className="text-muted-foreground text-xs">IP</div>
            <div className="tabular-nums">{status.ip || "-"}</div>
          </div>
          <div>
            <div className="text-muted-foreground text-xs">Signal</div>
            <div className="tabular-nums">{status.rssi} dBm</div>
          </div>
        </div>
      </Panel>

      <Panel title="Saved networks">
        {status.saved.length === 0 ? (
          <p className="text-sm text-muted-foreground">No saved networks.</p>
        ) : (
          <ul className="flex flex-col gap-1">
            {status.saved.map((n) => {
              const isConnected = status.mode === "station" && status.ssid === n.ssid
              return (
                <li key={n.ssid} className="flex items-center justify-between rounded-md border border-border px-3 py-1.5 text-sm">
                  <span>{n.ssid}</span>
                  <div className="flex items-center gap-2">
                    {n.pref && <Badge tone="info">Preferred</Badge>}
                    {isConnected ? (
                      <Badge tone="good">Connected</Badge>
                    ) : (
                      <Button variant="secondary" disabled={connecting} onClick={() => connectSaved(n.ssid)}>
                        Connect
                      </Button>
                    )}
                    <Button variant="ghost" disabled={connecting} onClick={() => removeSaved(n.ssid)}>
                      Remove
                    </Button>
                  </div>
                </li>
              )
            })}
          </ul>
        )}
      </Panel>

      <Panel
        title="Scan for networks"
        action={
          <Button onClick={startScan} disabled={scanning}>
            {scanning ? "Scanning..." : "Scan"}
          </Button>
        }
      >
        {!scan && <p className="text-sm text-muted-foreground">No scan results yet.</p>}
        {scan && (
          <ul className="flex flex-col gap-1">
            {scan.aps.map((ap) => (
              <li key={ap.ssid} className="flex items-center justify-between rounded-md border border-border px-3 py-1.5 text-sm">
                <button className="text-left hover:underline" onClick={() => setSsid(ap.ssid)}>
                  {ap.ssid}
                </button>
                <span className="flex items-center gap-2 text-xs text-muted-foreground">
                  {ap.locked ? "secured" : "open"} · {ap.rssi} dBm
                </span>
              </li>
            ))}
          </ul>
        )}
      </Panel>

      <Panel title="Join a network">
        <div className="flex flex-col gap-3">
          <Field label="SSID">
            <TextInput value={ssid} onChange={(e) => setSsid(e.target.value)} />
          </Field>
          <Field label="Password">
            <TextInput type="password" value={password} onChange={(e) => setPassword(e.target.value)} />
          </Field>
          <Button variant="primary" disabled={joining || !ssid} onClick={join}>
            Join
          </Button>
        </div>
      </Panel>
    </div>
  )
}
