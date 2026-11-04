export interface AxisStatus {
  pos: number
  moving: boolean
  enabled: boolean
}

export interface GearStatus {
  pitchDu: number
  axis: "z" | "x"
  stopAfterDu: number
}

export type MachineState = "idle" | "running" | "flashing" | "estop"

export interface StatusPush {
  z: AxisStatus
  x: AxisStatus
  rpm: number
  angleDeg: number
  state: MachineState
  mode: string
  running: boolean
  units: "mm" | "inch"
  jogStepDu: number
  gear: GearStatus
  probeContact: boolean
}

export type CmdOutcome =
  | { status: "ok"; msg: string; data?: unknown }
  | { status: "denied"; msg: string }
  | { status: "unconfirmed" }

export interface ScanResult {
  aps: { ssid: string; rssi: number; locked: boolean }[]
}

export type WifiMode = "off" | "connecting" | "station" | "ap" | "disconnected"

export interface SavedNetwork {
  ssid: string
  pref: boolean
}

export interface WifiPush {
  mode: WifiMode
  ip: string
  rssi: number
  ssid: string
  apName: string
  enabled: boolean
  fallback: boolean
  savedCount: number
  saved: SavedNetwork[]
}
