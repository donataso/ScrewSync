import { describe, expect, it } from "vitest"
import { MdiQueue, replyText, type Entry } from "./mdi-queue"
import type { CmdOutcome } from "./types"

function setup() {
  const sent: string[] = []
  const resolvers: ((o: CmdOutcome) => void)[] = []
  let entries: Entry[] = []
  const queue = new MdiQueue(
    (line) => {
      sent.push(line)
      return new Promise<CmdOutcome>((resolve) => resolvers.push(resolve))
    },
    (e) => {
      entries = e
    },
  )
  const stages = () => entries.filter((e) => e.status !== "print").map((e) => `${e.line}:${e.stage ?? "-"}`)
  const view = () => entries.map((e) => `${e.line}|${e.status}|${e.reply}`)
  const reply = async (index: number, outcome: CmdOutcome) => {
    resolvers[index](outcome)
    await new Promise((r) => setTimeout(r, 0))
  }
  return { queue, sent, view, stages, reply, entries: () => entries }
}

const OK: CmdOutcome = { status: "ok", msg: "", data: {} }
const freeOk = (n: number, steps = 0, done = 0): CmdOutcome => ({
  status: "ok",
  msg: "",
  data: { msg: `free=${n} steps=${steps} done=${done}` },
})
const FULL: CmdOutcome = { status: "denied", msg: "queue full" }
const ERR: CmdOutcome = { status: "denied", msg: "bad word" }
const LOST: CmdOutcome = { status: "unconfirmed" }

describe("replyText", () => {
  it("maps outcomes to status text", () => {
    expect(replyText(OK)).toEqual({ reply: "ok", ok: true })
    expect(replyText({ status: "ok", msg: "", data: { msg: "5.0" } })).toEqual({ reply: "ok 5.0", ok: true })
    expect(replyText(ERR)).toEqual({ reply: "err bad word", ok: false })
    expect(replyText(LOST)).toEqual({ reply: "no reply", ok: false })
  })
})

describe("MdiQueue free slots", () => {
  it("sends further lines while slots are free and stops at 0", async () => {
    const t = setup()
    t.queue.enqueue(["G0 Z1", "G0 Z2", "G0 Z3"])
    await t.reply(0, freeOk(2))
    expect(t.sent).toEqual(["G0 Z1", "G0 Z2"])
    await t.reply(1, freeOk(0))
    expect(t.sent).toEqual(["G0 Z1", "G0 Z2"])
    expect(t.view()).toEqual(["G0 Z1|replied|ok", "G0 Z2|replied|ok", "G0 Z3|queued|"])
  })

  it("resumes when a status push shows room", async () => {
    const t = setup()
    t.queue.enqueue(["G0 Z1", "G0 Z2"])
    await t.reply(0, freeOk(0))
    t.queue.setFree(0, 0)
    expect(t.sent).toEqual(["G0 Z1"])
    t.queue.setFree(2, 0)
    expect(t.sent).toEqual(["G0 Z1", "G0 Z2"])
  })

  it("keeps a queue-full line pending and resends it when more room shows", async () => {
    const t = setup()
    t.queue.enqueue(["G0 Z1", "G1 Z2"])
    await t.reply(0, freeOk(1))
    await t.reply(1, FULL)
    expect(t.view()).toEqual(["G0 Z1|replied|ok", "G1 Z2|queued|"])
    t.queue.setFree(1, 0)
    expect(t.sent).toEqual(["G0 Z1", "G1 Z2"])
    t.queue.setFree(3, 0)
    expect(t.sent).toEqual(["G0 Z1", "G1 Z2", "G1 Z2"])
    await t.reply(2, freeOk(1))
    expect(t.view()).toEqual(["G0 Z1|replied|ok", "G1 Z2|replied|ok"])
  })

  it("still skips the rest on a real err", async () => {
    const t = setup()
    t.queue.enqueue(["G0 Z1", "G0 Z2", "G0 Z3"])
    await t.reply(0, freeOk(3))
    await t.reply(1, ERR)
    expect(t.view()).toEqual(["G0 Z1|replied|ok", "G0 Z2|replied|err bad word", "G0 Z3|skipped|"])
  })

  it("hides the free count in the status text", () => {
    expect(replyText(freeOk(2))).toEqual({ reply: "ok", ok: true })
  })
})

describe("MdiQueue print and progress", () => {
  it("places output under its PRINT row with rows before and after", async () => {
    const t = setup()
    t.queue.enqueue(["G54", "(PRINT, a)", "G55"])
    await t.reply(0, freeOk(3))
    await t.reply(1, freeOk(3))
    await t.reply(2, freeOk(3))
    t.queue.addPrint("a out")
    expect(t.view()).toEqual(["G54|replied|ok", "(PRINT, a)|replied|ok", "a out|print|", "G55|replied|ok"])
  })

  it("places output that arrives before its reply", async () => {
    const t = setup()
    t.queue.enqueue(["G54", "(PRINT, a)", "G55"])
    await t.reply(0, freeOk(3))
    t.queue.addPrint("a out")
    await t.reply(1, freeOk(3))
    await t.reply(2, freeOk(3))
    expect(t.view()).toEqual(["G54|replied|ok", "(PRINT, a)|replied|ok", "a out|print|", "G55|replied|ok"])
  })

  it("matches two outputs in send order with rows around them", async () => {
    const t = setup()
    t.queue.enqueue(["G54", "(PRINT, a)", "G55", "(PRINT, b)", "G56"])
    for (let i = 0; i < 5; i++) {
      await t.reply(i, freeOk(3))
    }
    t.queue.addPrint("a out")
    t.queue.addPrint("b out")
    expect(t.view()).toEqual([
      "G54|replied|ok",
      "(PRINT, a)|replied|ok",
      "a out|print|",
      "G55|replied|ok",
      "(PRINT, b)|replied|ok",
      "b out|print|",
      "G56|replied|ok",
    ])
  })

  it("does not register a queue-full PRINT twice", async () => {
    const t = setup()
    t.queue.enqueue(["G54", "(PRINT, a)", "G55"])
    await t.reply(0, freeOk(1))
    await t.reply(1, FULL)
    t.queue.setFree(3, 0)
    expect(t.sent).toEqual(["G54", "(PRINT, a)", "(PRINT, a)"])
    await t.reply(2, freeOk(2))
    await t.reply(3, freeOk(2))
    t.queue.addPrint("a out")
    t.queue.addPrint("other")
    expect(t.view()).toEqual(["G54|replied|ok", "(PRINT, a)|replied|ok", "a out|print|", "G55|replied|ok", "other|print|"])
  })

  it("stops expecting output from a PRINT row that was in flight at sendNow", async () => {
    const t = setup()
    t.queue.enqueue(["(PRINT, a)"])
    t.queue.sendNow("stop")
    await t.reply(0, freeOk(3))
    t.queue.addPrint("late")
    expect(t.view()).toEqual(["(PRINT, a)|skipped|", "stop|sending|", "late|print|"])
  })

  it("is not fooled by free dropping before the reply", async () => {
    const t = setup()
    t.queue.setFree(4, 0)
    t.queue.enqueue(["G54", "G0 Z1", "G55"])
    await t.reply(0, freeOk(4, 0, 0))
    t.queue.setFree(3, 0)
    await t.reply(1, freeOk(3, 1, 0))
    await t.reply(2, freeOk(3, 0, 0))
    expect(t.stages()).toEqual(["G54:done", "G0 Z1:running", "G55:done"])
  })

  it("tracks running and done lines across several mdiDone pushes", async () => {
    const t = setup()
    t.queue.setFree(4, 10)
    t.queue.enqueue(["G54", "G0 Z1", "G21", "G1 Z2 (PRINT, x)", "G55"])
    await t.reply(0, freeOk(4, 0, 10))
    await t.reply(1, freeOk(3, 1, 10))
    await t.reply(2, freeOk(3, 0, 10))
    await t.reply(3, freeOk(1, 2, 10))
    await t.reply(4, freeOk(1, 0, 10))
    expect(t.stages()).toEqual(["G54:done", "G0 Z1:running", "G21:done", "G1 Z2 (PRINT, x):accepted", "G55:done"])
    t.queue.setFree(2, 11)
    expect(t.stages()).toEqual(["G54:done", "G0 Z1:done", "G21:done", "G1 Z2 (PRINT, x):running", "G55:done"])
    t.queue.setFree(3, 12)
    expect(t.stages()[3]).toBe("G1 Z2 (PRINT, x):running")
    t.queue.setFree(4, 13)
    expect(t.stages()[3]).toBe("G1 Z2 (PRINT, x):done")
  })

  it("starts a later line behind unfinished earlier ones", async () => {
    const t = setup()
    t.queue.setFree(4, 5)
    t.queue.enqueue(["G0 Z1", "G0 Z2"])
    await t.reply(0, freeOk(3, 1, 5))
    t.queue.setFree(4, 6)
    await t.reply(1, freeOk(3, 1, 6))
    expect(t.stages()).toEqual(["G0 Z1:done", "G0 Z2:running"])
  })

  it("treats a reply without steps or done as done at once", async () => {
    const t = setup()
    t.queue.enqueue(["home", "G54"])
    await t.reply(0, OK)
    await t.reply(1, freeOk(4, 0, 0))
    expect(t.stages()).toEqual(["home:done", "G54:done"])
  })

  it("clears the running mark on sendNow", async () => {
    const t = setup()
    t.queue.setFree(4, 0)
    t.queue.enqueue(["G54", "G0 Z1", "G55"])
    await t.reply(0, freeOk(4, 0, 0))
    await t.reply(1, freeOk(3, 1, 0))
    await t.reply(2, freeOk(3, 0, 0))
    t.queue.sendNow("stop")
    expect(t.stages()).toEqual(["G54:done", "G0 Z1:-", "G55:done", "stop:-"])
    t.queue.setFree(4, 1)
    expect(t.stages()).toEqual(["G54:done", "G0 Z1:-", "G55:done", "stop:-"])
  })
})

describe("MdiQueue print placement", () => {
  it("puts output under its PRINT row even after later rows exist", async () => {
    const t = setup()
    t.queue.enqueue(["(PRINT, a)", "G54"])
    await t.reply(0, freeOk(3))
    await t.reply(1, freeOk(3))
    t.queue.addPrint("a out")
    expect(t.view()).toEqual(["(PRINT, a)|replied|ok", "a out|print|", "G54|replied|ok"])
  })

  it("puts output under a PRINT row that has rows before and after it", async () => {
    const t = setup()
    t.queue.enqueue(["G0 Z2", "(PRINT, a)", "G54"])
    await t.reply(0, freeOk(3))
    await t.reply(1, freeOk(2))
    await t.reply(2, freeOk(1))
    t.queue.addPrint("a out")
    expect(t.view()).toEqual(["G0 Z2|replied|ok", "(PRINT, a)|replied|ok", "a out|print|", "G54|replied|ok"])
  })

  it("matches outputs to PRINT rows in order", async () => {
    const t = setup()
    t.queue.enqueue(["(print, one)", "G54", "(PRINT, two)"])
    await t.reply(0, freeOk(3))
    await t.reply(1, freeOk(3))
    await t.reply(2, freeOk(3))
    t.queue.addPrint("1")
    t.queue.addPrint("2")
    expect(t.view()).toEqual(["(print, one)|replied|ok", "1|print|", "G54|replied|ok", "(PRINT, two)|replied|ok", "2|print|"])
  })

  it("puts an unmatched print at the bottom", async () => {
    const t = setup()
    t.queue.enqueue(["G54"])
    await t.reply(0, freeOk(3))
    t.queue.addPrint("job")
    expect(t.view()).toEqual(["G54|replied|ok", "job|print|"])
  })

  it("does not expect output from a refused PRINT row", async () => {
    const t = setup()
    t.queue.enqueue(["(PRINT, x #5241)", "G54"])
    await t.reply(0, { status: "denied", msg: "unsupported parameter" })
    t.queue.addPrint("other")
    expect(t.view()).toEqual(["(PRINT, x #5241)|replied|err unsupported parameter", "G54|skipped|", "other|print|"])
  })

  it("stops expecting output after sendNow or clear", async () => {
    const t = setup()
    t.queue.enqueue(["(PRINT, a)", "G54"])
    await t.reply(0, freeOk(3))
    await t.reply(1, freeOk(3))
    t.queue.sendNow("stop")
    t.queue.addPrint("late")
    expect(t.view()).toEqual(["(PRINT, a)|replied|ok", "G54|replied|ok", "stop|sending|", "late|print|"])
    const u = setup()
    u.queue.enqueue(["(PRINT, a)"])
    await u.reply(0, freeOk(3))
    u.queue.clear()
    u.queue.addPrint("late")
    expect(u.view()).toEqual(["late|print|"])
  })

  it("keeps expecting output when a later line fails", async () => {
    const t = setup()
    t.queue.enqueue(["(PRINT, a)", "G0 Z1"])
    await t.reply(0, freeOk(3))
    await t.reply(1, ERR)
    t.queue.addPrint("a out")
    expect(t.view()).toEqual(["(PRINT, a)|replied|ok", "a out|print|", "G0 Z1|replied|err bad word"])
  })
})

describe("MdiQueue", () => {
  it("queues all lines at once and sends one at a time", async () => {
    const t = setup()
    t.queue.enqueue(["G90", "#1 = 2", "G0 Z1"])
    expect(t.view()).toEqual(["G90|sending|", "#1 = 2|queued|", "G0 Z1|queued|"])
    expect(t.sent).toEqual(["G90"])
    await t.reply(0, OK)
    expect(t.sent).toEqual(["G90", "#1 = 2"])
    expect(t.view()).toEqual(["G90|replied|ok", "#1 = 2|sending|", "G0 Z1|queued|"])
    await t.reply(1, { status: "ok", msg: "", data: { msg: "done" } })
    await t.reply(2, OK)
    expect(t.view()).toEqual(["G90|replied|ok", "#1 = 2|replied|ok done", "G0 Z1|replied|ok"])
  })

  it("skips the rest after an err reply", async () => {
    const t = setup()
    t.queue.enqueue(["G0 Z1", "G0 Z2", "G0 Z3"])
    await t.reply(0, ERR)
    expect(t.view()).toEqual(["G0 Z1|replied|err bad word", "G0 Z2|skipped|", "G0 Z3|skipped|"])
    expect(t.entries()[0].ok).toBe(false)
    expect(t.sent).toEqual(["G0 Z1"])
  })

  it("skips the rest when no reply arrives", async () => {
    const t = setup()
    t.queue.enqueue(["G0 Z1", "G0 Z2"])
    await t.reply(0, LOST)
    expect(t.view()).toEqual(["G0 Z1|replied|no reply", "G0 Z2|skipped|"])
    expect(t.sent).toEqual(["G0 Z1"])
  })

  it("sends a later batch after an error", async () => {
    const t = setup()
    t.queue.enqueue(["G0 Z1", "G0 Z2"])
    await t.reply(0, ERR)
    t.queue.enqueue(["G0 Z3"])
    expect(t.sent).toEqual(["G0 Z1", "G0 Z3"])
  })

  it("keeps a line queued behind one in flight", async () => {
    const t = setup()
    t.queue.enqueue(["home"])
    t.queue.enqueue(["G54"])
    expect(t.sent).toEqual(["home"])
    await t.reply(0, OK)
    expect(t.sent).toEqual(["home", "G54"])
  })

  it("sendNow sends at once and skips queued and in-flight lines", async () => {
    const t = setup()
    t.queue.enqueue(["G0 Z1", "G0 Z2", "G0 Z3"])
    t.queue.sendNow("stop")
    expect(t.sent).toEqual(["G0 Z1", "stop"])
    expect(t.view()).toEqual(["G0 Z1|skipped|", "G0 Z2|skipped|", "G0 Z3|skipped|", "stop|sending|"])
    await t.reply(0, OK)
    expect(t.view()[0]).toBe("G0 Z1|skipped|")
    expect(t.sent).toEqual(["G0 Z1", "stop"])
    await t.reply(1, OK)
    expect(t.view()[3]).toBe("stop|replied|ok")
  })

  it("sendNow with an empty queue just sends", async () => {
    const t = setup()
    t.queue.sendNow("estop on")
    expect(t.sent).toEqual(["estop on"])
    await t.reply(0, ERR)
    expect(t.view()).toEqual(["estop on|replied|err bad word"])
  })

  it("accepts new lines after sendNow", async () => {
    const t = setup()
    t.queue.enqueue(["G0 Z1"])
    t.queue.sendNow("stop")
    t.queue.enqueue(["G0 Z2"])
    expect(t.sent).toEqual(["G0 Z1", "stop"])
    await t.reply(0, OK)
    expect(t.sent).toEqual(["G0 Z1", "stop", "G0 Z2"])
  })

  it("adds print rows without touching the queue", async () => {
    const t = setup()
    t.queue.enqueue(["(PRINT, hi)", "G0 Z1"])
    t.queue.addPrint("hi")
    expect(t.view()).toEqual(["(PRINT, hi)|sending|", "hi|print|", "G0 Z1|queued|"])
    await t.reply(0, OK)
    expect(t.sent).toEqual(["(PRINT, hi)", "G0 Z1"])
  })

  it("clear empties the log and drops queued lines", async () => {
    const t = setup()
    t.queue.enqueue(["G0 Z1", "G0 Z2"])
    t.queue.clear()
    expect(t.view()).toEqual([])
    await t.reply(0, OK)
    expect(t.view()).toEqual([])
    expect(t.sent).toEqual(["G0 Z1"])
  })
})
