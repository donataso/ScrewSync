"use client"

import { createContext, useContext, useEffect, useState } from "react"
import type { StatusPush } from "./types"
import { wsConnect, wsSubscribe } from "./ws"

interface StateContextValue {
  state: StatusPush | null
  error: string | null
  connected: boolean
}

const StateContext = createContext<StateContextValue | null>(null)

export function StateProvider({ children }: { children: React.ReactNode }) {
  const [state, setState] = useState<StatusPush | null>(null)
  const [error, setError] = useState<string | null>(null)
  const [connected, setConnected] = useState(false)

  useEffect(() => {
    wsConnect()
    const unsubscribe = wsSubscribe({
      onStatus: (next) => setState(next),
      onConnected: (isConnected, err) => {
        setConnected(isConnected)
        setError(err)
      },
    })
    return unsubscribe
  }, [])

  return <StateContext.Provider value={{ state, error, connected }}>{children}</StateContext.Provider>
}

export function useDeviceState(): StateContextValue {
  const ctx = useContext(StateContext)
  if (!ctx) {
throw new Error("useDeviceState must be used within StateProvider")
}
  return ctx
}

// Mirrors the firmware's refuseUnlessIdle(): Idle state and no mode running.
export function useCanConfigure(): boolean {
  const ctx = useContext(StateContext)
  if (!ctx) {
throw new Error("useCanConfigure must be used within StateProvider")
}
  return ctx.state !== null && ctx.state.state === "idle" && !ctx.state.running
}

export const CONFIGURE_LOCK_MESSAGE = "Only allowed while the machine is idle."
