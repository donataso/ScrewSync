"use client"

import { createContext, useContext, useEffect, useState } from "react"
import { inputLevels, type InputLevels } from "./pins"
import type { Capabilities, StatusPush } from "./types"
import { wsConnect, wsInputs, wsSubscribe } from "./ws"

interface StateContextValue {
  state: StatusPush | null
  error: string | null
  connected: boolean
  capabilities: Capabilities | null
  inputs: InputLevels
  setInputs: (inputs: InputLevels) => void
}

const StateContext = createContext<StateContextValue | null>(null)

export function StateProvider({ children }: { children: React.ReactNode }) {
  const [state, setState] = useState<StatusPush | null>(null)
  const [error, setError] = useState<string | null>(null)
  const [connected, setConnected] = useState(false)
  const [capabilities, setCapabilities] = useState<Capabilities | null>(null)
  const [inputs, setInputs] = useState<InputLevels>({})

  useEffect(() => {
    wsConnect()
    const unsubscribe = wsSubscribe({
      onStatus: (next) => setState(next),
      onCapabilities: setCapabilities,
      onInputs: (next) => setInputs(inputLevels(next)),
      onConnected: (isConnected, err) => {
        setConnected(isConnected)
        if (!isConnected) {
          setInputs({})
        }
        setError(err)
      },
    })
    return unsubscribe
  }, [])

  return <StateContext.Provider value={{ state, error, connected, capabilities, inputs, setInputs }}>{children}</StateContext.Provider>
}

export function useDeviceState(): StateContextValue {
  const ctx = useContext(StateContext)
  if (!ctx) {
throw new Error("useDeviceState must be used within StateProvider")
}
  return ctx
}

// Mirrors the firmware's refuseUnlessIdle(): Idle or estop state and no mode running.
export function useCanConfigure(): boolean {
  const ctx = useContext(StateContext)
  if (!ctx) {
throw new Error("useCanConfigure must be used within StateProvider")
}
  return ctx.state !== null && (ctx.state.state === "idle" || ctx.state.state === "estop") && !ctx.state.running
}

export const CONFIGURE_LOCK_MESSAGE = "Only allowed while the machine is idle."

export function useInputLevels(): InputLevels {
  const { inputs, setInputs } = useDeviceState()
  useEffect(() => {
    wsInputs(true)
    const unsubscribe = wsSubscribe({
      onConnected: (connected) => {
        if (connected) {
          wsInputs(true)
        }
      },
    })
    return () => {
      unsubscribe()
      wsInputs(false)
      setInputs({})
    }
  }, [setInputs])
  return inputs
}
