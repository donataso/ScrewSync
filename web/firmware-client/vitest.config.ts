import { defineConfig } from "vitest/config"

// Test files live under ../client (see vite.config.ts's alias) -- this
// project has no src/ of its own, it just bundles ../client.
export default defineConfig({
  test: {
    include: ["../client/**/*.test.ts"],
  },
})
