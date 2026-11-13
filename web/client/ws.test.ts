import { describe, expect, it } from "vitest"
import { expectedReplyType } from "./ws"

describe("expectedReplyType", () => {
  it("waits for the push of the command's domain", () => {
    expect(expectedReplyType("settings set encoderPpr 600")).toBe("settings")
    expect(expectedReplyType("display flash")).toBe("display")
    expect(expectedReplyType("wifi status")).toBe("wifi")
    expect(expectedReplyType("wifi add home secret")).toBe("wifi")
    expect(expectedReplyType("wifi scan")).toBe("scan")
  })

  it("waits for a status push for every other command", () => {
    expect(expectedReplyType("jog z +")).toBe("status")
    expect(expectedReplyType("stop")).toBe("status")
    expect(expectedReplyType("settingsx")).toBe("status")
    expect(expectedReplyType("")).toBe("status")
  })
})
