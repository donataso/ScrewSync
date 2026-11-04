"use client"

import { createContext, useCallback, useContext, useState } from "react"

type ToastTone = "good" | "bad" | "warn" | "info"

interface ToastEntry {
  id: number
  msg: string
  tone: ToastTone
}

interface ToastContextValue {
  push: (msg: string, tone?: ToastTone) => void
}

const ToastContext = createContext<ToastContextValue | null>(null)

const LIFETIME_MS = 4000

let nextId = 1

export function ToastProvider({ children }: { children: React.ReactNode }) {
  const [toasts, setToasts] = useState<ToastEntry[]>([])

  const dismiss = useCallback((id: number) => {
    setToasts((t) => t.filter((x) => x.id !== id))
  }, [])

  const push = useCallback(
    (msg: string, tone: ToastTone = "info") => {
      const id = nextId++
      setToasts((t) => [...t, { id, msg, tone }])
      setTimeout(() => dismiss(id), LIFETIME_MS)
    },
    [dismiss],
  )

  return (
    <ToastContext.Provider value={{ push }}>
      {children}
      {toasts.length > 0 && (
        <div className="pointer-events-none fixed left-1/2 top-4 z-50 flex max-w-[90vw] -translate-x-1/2 flex-col gap-2">
          {toasts.map((t) => (
            <div
              key={t.id}
              onClick={() => dismiss(t.id)}
              className={`pointer-events-auto flex max-w-md items-center gap-2 rounded-md border bg-card px-3 py-2 text-sm font-medium shadow-lg ${TONE_CLASSES[t.tone]}`}
            >
              <span className="flex-1 text-left">{t.msg}</span>
              <button
                type="button"
                aria-label="Copy"
                onClick={(e) => {
                  e.stopPropagation()
                  copyText(t.msg)
                }}
                className="shrink-0 opacity-70 hover:opacity-100"
              >
                <CopyIcon />
              </button>
            </div>
          ))}
        </div>
      )}
    </ToastContext.Provider>
  )
}

const TONE_CLASSES: Record<ToastTone, string> = {
  good: "border-emerald-500/40 text-emerald-600 dark:text-emerald-400",
  warn: "border-amber-500/40 text-amber-600 dark:text-amber-400",
  bad: "border-red-500/40 text-red-600 dark:text-red-400",
  info: "border-sky-500/40 text-sky-600 dark:text-sky-400",
}

function copyText(text: string) {
  if (navigator.clipboard) {
    navigator.clipboard.writeText(text)
    return
  }
  const ta = document.createElement("textarea")
  ta.value = text
  ta.style.position = "fixed"
  ta.style.opacity = "0"
  document.body.appendChild(ta)
  ta.select()
  document.execCommand("copy")
  document.body.removeChild(ta)
}

function CopyIcon() {
  return (
    <svg width="14" height="14" viewBox="0 0 24 24" fill="none" stroke="currentColor" strokeWidth="2" strokeLinecap="round" strokeLinejoin="round">
      <rect x="9" y="9" width="13" height="13" rx="2" />
      <path d="M5 15H4a2 2 0 0 1-2-2V4a2 2 0 0 1 2-2h9a2 2 0 0 1 2 2v1" />
    </svg>
  )
}

export function useToast(): ToastContextValue {
  const ctx = useContext(ToastContext)
  if (!ctx) throw new Error("useToast must be used within ToastProvider")
  return ctx
}
