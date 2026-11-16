import { writeFileSync } from "node:fs"
import path from "node:path"
import tailwindcss from "@tailwindcss/vite"
import react from "@vitejs/plugin-react"
import { defineConfig, type Plugin } from "vite"
import { viteSingleFile } from "vite-plugin-singlefile"

function noticeModules(file: string): Plugin {
  return {
    name: "notice-modules",
    generateBundle(_, bundle) {
      const ids = new Set<string>()
      for (const item of Object.values(bundle)) {
        if (item.type === "chunk") Object.keys(item.modules).forEach((id) => ids.add(id))
      }
      writeFileSync(file, JSON.stringify([...ids].sort()))
    },
  }
}

// scripts/build_webui.py runs this in a container with only firmware-client/
// and client/ mounted (see its docstring) -- it can't see ../firmware, so it
// reads DEVICE_NAME from config.hpp itself and passes it in via env instead
// of this config reading the file directly like next.config.mjs does.
export default defineConfig({
  plugins: [
    react(),
    tailwindcss(),
    ...(process.env.NOTICE_MODULES ? [noticeModules(process.env.NOTICE_MODULES)] : []),
    viteSingleFile(),
  ],
  define: {
    __DEVICE_NAME__: JSON.stringify(process.env.DEVICE_NAME || "ScrewSync"),
  },
  resolve: {
    alias: [{ find: "@/client", replacement: path.resolve(__dirname, "../client") }],
    dedupe: ["lucide-react"],
  },
  server: process.env.DEVICE_HOST
    ? {
        host: "0.0.0.0",
        proxy: {
          "^/(ws|wifi|update|update-display|display-preview|threads|pins|backup|log|gcode)(/.*)?$": {
            target: `http://${process.env.DEVICE_HOST}`,
            changeOrigin: true,
            ws: true,
          },
        },
      }
    : undefined,
  build: {
    target: "es2018",
    cssCodeSplit: false,
    assetsInlineLimit: 100_000_000,
    outDir: process.env.OUT_DIR || path.resolve(__dirname, "../../.build/firmware-client"),
    emptyOutDir: true,
  },
})
