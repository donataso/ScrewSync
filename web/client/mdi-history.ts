// MDI console history, shell style: Up walks back, Down walks forward and
// past the newest line returns to the unsent draft.

export const HISTORY_MAX = 200
const STORAGE_KEY = "screwsync.mdiHistory"

export function addToHistory(history: string[], line: string): string[] {
  const trimmed = line.trim()
  if (!trimmed || history[history.length - 1] === trimmed) {
    return history
  }
  return [...history, trimmed].slice(-HISTORY_MAX)
}

export interface Recall {
  index: number // history.length = the draft
  draft: string
}

export function freshRecall(history: string[]): Recall {
  return { index: history.length, draft: "" }
}

// The recall state and input text after one step; dir -1 = older.
export function stepHistory(history: string[], recall: Recall, current: string, dir: -1 | 1): { recall: Recall; text: string } {
  const index = Math.min(Math.max(recall.index + dir, 0), history.length)
  const draft = recall.index === history.length ? current : recall.draft
  return { recall: { index, draft }, text: index === history.length ? draft : history[index] }
}

export function loadHistory(): string[] {
  try {
    const parsed: unknown = JSON.parse(window.localStorage.getItem(STORAGE_KEY) ?? "[]")
    return Array.isArray(parsed) ? parsed.filter((l): l is string => typeof l === "string").slice(-HISTORY_MAX) : []
  } catch {
    return []
  }
}

export function saveHistory(history: string[]): void {
  try {
    window.localStorage.setItem(STORAGE_KEY, JSON.stringify(history))
  } catch {
  }
}
