import fs from "node:fs"
import path from "node:path"
import { fileURLToPath } from "node:url"

// Read at build time by both bundlers (vite.config.ts and next.config.mjs)
// so the web UI's title always matches main/src/config.hpp's
// DEVICE_NAME without hand-duplicating it.
const CONFIG_HPP = path.resolve(path.dirname(fileURLToPath(import.meta.url)), "../main/src/config.hpp")

export function readDeviceName() {
  const src = fs.readFileSync(CONFIG_HPP, "utf8")
  const m = /#define\s+DEVICE_NAME\s+"([^"]*)"/.exec(src)
  if (!m) throw new Error(`DEVICE_NAME not found in ${CONFIG_HPP}`)
  return m[1]
}
