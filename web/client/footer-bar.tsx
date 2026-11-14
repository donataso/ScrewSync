"use client"

import { createContext, useContext, useEffect, useMemo, useState, type ReactNode } from "react"
import { Button } from "./ui/primitives"

const FooterBarContext = createContext<{ setContent: (n: ReactNode | null) => void } | null>(null)

export function FooterBarProvider({ children }: { children: ReactNode }) {
  const [content, setContent] = useState<ReactNode | null>(null)
  const value = useMemo(() => ({ setContent }), [])
  return (
    <FooterBarContext.Provider value={value}>
      {children}
      {content && <div className="shrink-0 border-t border-border bg-background">{content}</div>}
    </FooterBarContext.Provider>
  )
}

/** Shows `node` in the app shell's footer bar while the calling component is mounted. */
export function useFooterBar(node: ReactNode | null) {
  const ctx = useContext(FooterBarContext)
  useEffect(() => {
    ctx?.setContent(node)
    return () => ctx?.setContent(null)
  }, [ctx, node])
}

export function SaveBar({
  dirty,
  saveLabel = "Save",
  saveDisabled,
  discardDisabled,
  resetDisabled,
  onSave,
  onDiscard,
  onReset,
}: {
  dirty: boolean
  saveLabel?: string
  saveDisabled: boolean
  discardDisabled: boolean
  resetDisabled: boolean
  onSave: () => void
  onDiscard: () => void
  onReset: () => void
}) {
  return (
    <div className="mx-auto flex max-w-5xl flex-wrap items-center gap-2 px-4 py-3">
      <Button variant="ghost" disabled={resetDisabled} onClick={onReset}>
        Reset to defaults
      </Button>
      <div className="ml-auto flex flex-wrap items-center gap-2">
        {dirty && <span className="text-sm text-muted-foreground">Not saved</span>}
        <Button disabled={!dirty || discardDisabled} onClick={onDiscard}>
          Discard
        </Button>
        <Button variant="primary" disabled={!dirty || saveDisabled} onClick={onSave}>
          {saveLabel}
        </Button>
      </div>
    </div>
  )
}
