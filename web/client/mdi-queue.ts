import type { CmdOutcome } from "./types"

export type EntryStatus = "queued" | "sending" | "replied" | "skipped" | "print"

export type EntryStage = "accepted" | "running" | "done"

export interface Entry {
  id: number
  line: string
  status: EntryStatus
  reply: string
  ok: boolean
  stage?: EntryStage
}

interface Outstanding {
  id: number
  end: number
}

export interface QueueInfo {
  free: number
  steps: number
  done: number
}

interface QueuedLine {
  id: number
  line: string
}

const INFO_TOKENS = /\s*\b(free|steps|done)=\d+/g
const QUEUE_FULL = "queue full"
const PRINT_LINE = /^\(print/i

export function queueInfo(outcome: CmdOutcome): QueueInfo | null {
  const msg = outcome.status === "ok" ? (outcome.data as { msg?: unknown } | undefined)?.msg : undefined
  if (typeof msg !== "string") {
    return null
  }
  const field = (name: string) => new RegExp(`\\b${name}=(\\d+)`).exec(msg)?.[1]
  const free = field("free")
  return free === undefined ? null : { free: Number(free), steps: Number(field("steps") ?? 0), done: Number(field("done") ?? 0) }
}

export function replyText(outcome: CmdOutcome): { reply: string; ok: boolean } {
  if (outcome.status === "denied") {
    return { reply: `err ${outcome.msg}`, ok: false }
  }
  if (outcome.status === "unconfirmed") {
    return { reply: "no reply", ok: false }
  }
  const raw = (outcome.data as { msg?: unknown } | undefined)?.msg
  const msg = typeof raw === "string" ? raw.replace(INFO_TOKENS, "").trim() : ""
  return { reply: msg ? `ok ${msg}` : "ok", ok: true }
}

export class MdiQueue {
  private entries: Entry[] = []
  private queue: QueuedLine[] = []
  private busy = false
  private inFlight: number | null = null
  private abandoned = new Set<number>()
  private nextId = 0
  private free = 1
  private blockedAt: number | null = null
  private awaitingPrint: number[] = []
  private outstanding: Outstanding[] = []
  private doneCount = 0
  private lastEnd = 0

  constructor(
    private send: (line: string) => Promise<CmdOutcome>,
    private onChange: (entries: Entry[]) => void,
  ) {}

  enqueue(lines: string[]): void {
    const added = lines.map((line) => ({ id: this.nextId++, line }))
    this.queue.push(...added)
    this.update([...this.entries, ...added.map(({ id, line }): Entry => ({ id, line, status: "queued", reply: "", ok: true }))])
    void this.pump()
  }

  sendNow(line: string): void {
    this.skipUnfinished()
    this.awaitingPrint = []
    this.dropOutstanding()
    const id = this.nextId++
    this.update([...this.entries, { id, line, status: "sending", reply: "", ok: true }])
    void this.send(line).then((outcome) => {
      this.patch(id, { status: "replied", ...replyText(outcome) })
    })
  }

  setFree(free: number, done: number): void {
    this.doneCount = Math.max(this.doneCount, done)
    this.refreshStages()
    if (this.blockedAt !== null && free <= this.blockedAt) {
      return
    }
    this.blockedAt = null
    this.free = free
    void this.pump()
  }

  addPrint(text: string): void {
    const entry: Entry = { id: this.nextId++, line: text, status: "print", reply: "", ok: true }
    const ownerId = this.awaitingPrint.shift()
    const owner = this.entries.findIndex((e) => e.id === ownerId)
    const at = owner < 0 ? this.entries.length : owner + 1
    this.update([...this.entries.slice(0, at), entry, ...this.entries.slice(at)])
  }

  clear(): void {
    this.queue = []
    this.awaitingPrint = []
    this.outstanding = []
    this.lastEnd = 0
    if (this.inFlight !== null) {
      this.abandoned.add(this.inFlight)
    }
    this.update([])
  }

  private dropOutstanding(): void {
    const dropped = new Set(this.outstanding.map((o) => o.id))
    this.outstanding = []
    this.lastEnd = 0
    this.update(this.entries.map((e) => (dropped.has(e.id) ? { ...e, stage: undefined } : e)))
  }

  private refreshStages(): void {
    this.outstanding = this.outstanding.filter((o) => o.end > this.doneCount)
    const stageOf = (id: number): EntryStage => {
      const index = this.outstanding.findIndex((o) => o.id === id)
      return index < 0 ? "done" : index === 0 ? "running" : "accepted"
    }
    this.update(this.entries.map((e) => (e.stage === undefined ? e : { ...e, stage: stageOf(e.id) })))
  }

  private accept(id: number, info: QueueInfo | null): void {
    if (info) {
      this.doneCount = Math.max(this.doneCount, info.done)
      if (info.steps > 0) {
        this.lastEnd = Math.max(this.lastEnd, info.done) + info.steps
        this.outstanding.push({ id, end: this.lastEnd })
      }
    }
    this.patch(id, { stage: "accepted" })
    this.refreshStages()
  }

  private forgetPrint(id: number): void {
    this.awaitingPrint = this.awaitingPrint.filter((p) => p !== id)
  }

  private update(entries: Entry[]): void {
    this.entries = entries
    this.onChange(entries)
  }

  private patch(id: number, change: Partial<Entry>): void {
    this.update(this.entries.map((e) => (e.id === id ? { ...e, ...change } : e)))
  }

  private skipUnfinished(): void {
    const skipped = new Set(this.queue.map((q) => q.id))
    if (this.inFlight !== null) {
      skipped.add(this.inFlight)
      this.abandoned.add(this.inFlight)
    }
    this.queue = []
    this.update(this.entries.map((e) => (skipped.has(e.id) ? { ...e, status: "skipped" } : e)))
  }

  private async pump(): Promise<void> {
    if (this.busy) {
      return
    }
    this.busy = true
    while (this.queue.length && this.free > 0 && this.blockedAt === null) {
      const next = this.queue.shift()!
      const freeBefore = this.free
      this.patch(next.id, { status: "sending" })
      this.inFlight = next.id
      if (PRINT_LINE.test(next.line.trim())) {
        this.awaitingPrint.push(next.id)
      }
      const outcome = await this.send(next.line)
      this.inFlight = null
      const info = queueInfo(outcome)
      if (info) {
        this.free = info.free
      }
      if (this.abandoned.delete(next.id)) {
        continue
      }
      if (outcome.status === "denied" && outcome.msg === QUEUE_FULL) {
        this.forgetPrint(next.id)
        this.queue.unshift(next)
        this.blockedAt = freeBefore
        this.patch(next.id, { status: "queued" })
        continue
      }
      const { reply, ok } = replyText(outcome)
      this.patch(next.id, { status: "replied", reply, ok })
      if (ok) {
        this.accept(next.id, info)
      } else {
        this.forgetPrint(next.id)
        this.skipUnfinished()
      }
    }
    this.busy = false
  }
}
