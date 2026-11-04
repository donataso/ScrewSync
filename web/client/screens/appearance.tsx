"use client"

import { formatTime, useAppearance } from "../appearance"
import { Field, Panel, Select } from "../ui/primitives"

export function AppearanceScreen() {
  const appearance = useAppearance()
  return (
    <div className="flex flex-col gap-4">
      <h1 className="text-lg font-semibold">Appearance</h1>
      <p className="text-sm text-muted-foreground">Stored only in this browser - never sent to the device.</p>
      <Panel title="Display">
        <div className="grid gap-3 md:grid-cols-2">
          <Field label="Theme">
            <Select value={appearance.theme} onChange={(e) => appearance.setTheme(e.target.value as any)}>
              <option value="system">System</option>
              <option value="light">Light</option>
              <option value="dark">Dark</option>
            </Select>
          </Field>
          <Field label="Time format" hint={`Preview: ${formatTime(new Date(), appearance.timeFormat)}`}>
            <Select value={appearance.timeFormat} onChange={(e) => appearance.setTimeFormat(e.target.value as any)}>
              <option value="system">System</option>
              <option value="24h">24-hour</option>
              <option value="12h">12-hour (AM/PM)</option>
            </Select>
          </Field>
        </div>
      </Panel>
    </div>
  )
}
