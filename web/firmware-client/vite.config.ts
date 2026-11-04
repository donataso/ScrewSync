import path from "node:path"
import tailwindcss from "@tailwindcss/vite"
import react from "@vitejs/plugin-react"
import { defineConfig } from "vite"
import { viteSingleFile } from "vite-plugin-singlefile"

// scripts/build_webui.py runs this in a container with only firmware-client/
// and client/ mounted (see its docstring) -- it can't see ../firmware, so it
// reads DEVICE_NAME from config.hpp itself and passes it in via env instead
// of this config reading the file directly like next.config.mjs does.
export default defineConfig({
  plugins: [react(), tailwindcss(), viteSingleFile()],
  define: {
    __DEVICE_NAME__: JSON.stringify(process.env.DEVICE_NAME || "ScrewSync"),
  },
  resolve: {
    alias: [{ find: "@/client", replacement: path.resolve(__dirname, "../client") }],
  },
  build: {
    target: "es2018",
    cssCodeSplit: false,
    assetsInlineLimit: 100_000_000,
    outDir: process.env.OUT_DIR || path.resolve(__dirname, "../../.build/firmware-client"),
    emptyOutDir: true,
  },
})
