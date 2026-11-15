// The result push of one G38 move: "z found=1 pos=<du> motor=<du>".

export interface ProbeResult {
  axis: "Z" | "X"
  found: boolean
  posDu: number // work coordinates
  motorDu: number // motor position, without backlash compensation
}

export function parseProbe(msg: string): ProbeResult | null {
  const m = /^([zx]) found=([01]) pos=(-?\d+) motor=(-?\d+)$/.exec(msg)
  if (!m) {
    return null
  }
  return { axis: m[1] === "z" ? "Z" : "X", found: m[2] === "1", posDu: Number(m[3]), motorDu: Number(m[4]) }
}
