import { App } from "@/client/app"
import { RouterProvider } from "@/client/router"
import { StateProvider } from "@/client/state-provider"
import { StrictMode } from "react"
import { createRoot } from "react-dom/client"
import "./theme.css"

const root = document.getElementById("root")
if (!root) {
throw new Error("missing #root element")
}

createRoot(root).render(
  <StrictMode>
    <RouterProvider>
      <StateProvider>
        <App />
      </StateProvider>
    </RouterProvider>
  </StrictMode>,
)
