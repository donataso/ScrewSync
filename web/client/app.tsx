"use client"

import { useEffect } from "react"
import { AppearanceContext, useAppearanceState } from "./appearance"
import { FooterBarProvider } from "./footer-bar"
import { useRouter } from "./router"
import { useDeviceState } from "./state-provider"
import { AppearanceScreen } from "./screens/appearance"
import { DeviceScreen } from "./screens/device"
import { GcodeScreen } from "./screens/gcode"
import { NetworkScreen } from "./screens/network"
import { PinsScreen } from "./screens/pins"
import { SettingsScreen } from "./screens/settings"
import { ThreadsScreen } from "./screens/threads"
import { Badge } from "./ui/primitives"
import { ToastProvider } from "./ui/toast"

const NAV = [
  { path: "/settings", label: "Settings" },
  { path: "/threads", label: "Threads" },
  { path: "/gcode", label: "Gcode" },
  { path: "/network", label: "Network" },
  { path: "/pins", label: "Pins" },
  { path: "/device", label: "Device" },
  { path: "/appearance", label: "Appearance" },
]

function screenFor(path: string) {
  if (path.startsWith("/threads")) {
return <ThreadsScreen />
}
  if (path.startsWith("/gcode")) {
return <GcodeScreen />
}
  if (path.startsWith("/network")) {
return <NetworkScreen />
}
  if (path.startsWith("/pins")) {
return <PinsScreen />
}
  if (path.startsWith("/device")) {
return <DeviceScreen />
}
  if (path.startsWith("/appearance")) {
return <AppearanceScreen />
}
  return <SettingsScreen />
}

function availableWhenOffline(path: string) {
  return path.startsWith("/device") || path.startsWith("/appearance")
}

export function App() {
  const appearance = useAppearanceState()
  return (
    <AppearanceContext.Provider value={appearance}>
      <ToastProvider>
        <AppShell />
      </ToastProvider>
    </AppearanceContext.Provider>
  )
}

function AppShell() {
  const { path, navigate } = useRouter()
  const { connected, error } = useDeviceState()

  useEffect(() => {
    document.title = __DEVICE_NAME__
  }, [])

  return (
    <div className="flex h-screen flex-col bg-background text-foreground">
      <header className="shrink-0 border-b border-border">
        <div className="mx-auto flex max-w-5xl flex-wrap items-center justify-between gap-3 px-4 py-3">
          <div className="flex items-center gap-3">
            <div className="text-sm font-semibold">{__DEVICE_NAME__}</div>
            <Badge tone={connected ? "good" : "bad"}>{connected ? "online" : "offline"}</Badge>
          </div>
          <nav className="flex flex-wrap gap-1">
            {NAV.map((n) => {
              const active =
                n.path === "/settings"
                  ? path === "/" || path === n.path || path.startsWith(n.path + "/")
                  : path === n.path || path.startsWith(n.path + "/")
              return (
                <button
                  key={n.path}
                  onClick={() => navigate(n.path)}
                  className={`rounded-md px-3 py-1.5 text-sm font-medium transition-colors ${
                    active ? "bg-primary text-primary-foreground" : "text-muted-foreground hover:bg-muted hover:text-foreground"
                  }`}
                >
                  {n.label}
                </button>
              )
            })}
          </nav>
        </div>
      </header>
      {/* FooterBarProvider renders no element of its own -- the scroll div and
          a screen's footer bar (useFooterBar) end up as direct flex siblings
          here, so the bar reserves real height instead of overlaying the
          scroll area's bottom edge (and its native scrollbar). */}
      <FooterBarProvider>
        {/* flex-1 + overflow-y-auto here (full width) so the scrollbar sits at
            the window edge, not on the centered <main> below -- max-w-5xl
            there would otherwise inset it. */}
        <div className="flex-1 overflow-y-auto">
          <main className="mx-auto max-w-5xl px-4 py-6">
            {connected || availableWhenOffline(path) ? (
              screenFor(path)
            ) : (
              <div className="flex flex-col items-center gap-2 rounded-md border border-border p-10 text-center">
                <p className="text-lg font-semibold">Device offline</p>
                <p className="text-sm text-muted-foreground">{error ?? "Lost contact with the device."}</p>
              </div>
            )}
          </main>
        </div>
      </FooterBarProvider>
    </div>
  )
}
