"use client"

import { createContext, useCallback, useContext, useEffect, useState } from "react"

interface RouterContextValue {
  path: string
  navigate: (path: string) => void
}

const RouterContext = createContext<RouterContextValue | null>(null)

function currentPath(): string {
  if (typeof window === "undefined") {
return "/"
}
  const hash = window.location.hash.slice(1)
  return hash ? (hash.startsWith("/") ? hash : "/" + hash) : "/"
}

export function RouterProvider({ children }: { children: React.ReactNode }) {
  const [path, setPath] = useState<string>(currentPath())

  useEffect(() => {
    setPath(currentPath())
    const onHashChange = () => setPath(currentPath())
    window.addEventListener("hashchange", onHashChange)
    return () => window.removeEventListener("hashchange", onHashChange)
  }, [])

  const navigate = useCallback((to: string) => {
    if (to === currentPath()) {
return
}
    window.location.hash = to
    setPath(to)
  }, [])

  return <RouterContext.Provider value={{ path, navigate }}>{children}</RouterContext.Provider>
}

export function useRouter(): RouterContextValue {
  const ctx = useContext(RouterContext)
  if (!ctx) {
throw new Error("useRouter must be used within RouterProvider")
}
  return ctx
}
