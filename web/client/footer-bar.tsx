"use client"

import { createContext, useContext, useEffect, useMemo, useState, type ReactNode } from "react"

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
