import { App } from "@/client/app"
import { RouterProvider } from "@/client/router"
import { StateProvider } from "@/client/state-provider"

export default function Page() {
  return (
    <RouterProvider>
      <StateProvider>
        <App />
      </StateProvider>
    </RouterProvider>
  )
}
