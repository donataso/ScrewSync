"use client"

import { createContext, useContext, useEffect, useState } from "react"

export type ThemePref = "system" | "light" | "dark"
export type TimeFormatPref = "system" | "24h" | "12h"

const STORAGE_KEY = "screwsync.appearance"

interface AppearancePrefs {
  theme: ThemePref
  timeFormat: TimeFormatPref
}

const DEFAULT_PREFS: AppearancePrefs = { theme: "system", timeFormat: "system" }

function readPrefs(): AppearancePrefs {
  if (typeof window === "undefined") return DEFAULT_PREFS
  try {
    const raw = window.localStorage.getItem(STORAGE_KEY)
    if (!raw) return DEFAULT_PREFS
    const parsed = JSON.parse(raw)
    return { ...DEFAULT_PREFS, ...parsed }
  } catch {
    return DEFAULT_PREFS
  }
}

function writePrefs(prefs: AppearancePrefs) {
  try {
    window.localStorage.setItem(STORAGE_KEY, JSON.stringify(prefs))
  } catch {
  }
}

interface AppearanceContextValue extends AppearancePrefs {
  setTheme: (t: ThemePref) => void
  setTimeFormat: (f: TimeFormatPref) => void
  resolvedTheme: "light" | "dark"
}

export const AppearanceContext = createContext<AppearanceContextValue | null>(null)

export function useAppearanceState(): AppearanceContextValue {
  const [prefs, setPrefs] = useState<AppearancePrefs>(readPrefs)
  const [systemDark, setSystemDark] = useState(false)

  useEffect(() => {
    setPrefs(readPrefs())
    const mq = window.matchMedia("(prefers-color-scheme: dark)")
    setSystemDark(mq.matches)
    const onChange = (e: MediaQueryListEvent) => setSystemDark(e.matches)
    mq.addEventListener("change", onChange)
    return () => mq.removeEventListener("change", onChange)
  }, [])

  const resolvedTheme: "light" | "dark" = prefs.theme === "system" ? (systemDark ? "dark" : "light") : prefs.theme

  useEffect(() => {
    document.documentElement.classList.toggle("dark", resolvedTheme === "dark")
  }, [resolvedTheme])

  const setTheme = (theme: ThemePref) => {
    const next = { ...prefs, theme }
    setPrefs(next)
    writePrefs(next)
  }
  const setTimeFormat = (timeFormat: TimeFormatPref) => {
    const next = { ...prefs, timeFormat }
    setPrefs(next)
    writePrefs(next)
  }

  return { ...prefs, setTheme, setTimeFormat, resolvedTheme }
}

export function useAppearance(): AppearanceContextValue {
  const ctx = useContext(AppearanceContext)
  if (!ctx) throw new Error("useAppearance must be used within AppearanceContext.Provider")
  return ctx
}

export function formatTime(date: Date, pref: TimeFormatPref): string {
  if (pref === "24h") {
    return date.toLocaleTimeString(undefined, { hour: "2-digit", minute: "2-digit", second: "2-digit", hour12: false })
  }
  if (pref === "12h") {
    return date.toLocaleTimeString(undefined, { hour: "2-digit", minute: "2-digit", second: "2-digit", hour12: true })
  }
  return date.toLocaleTimeString()
}
