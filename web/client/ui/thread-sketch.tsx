"use client"

const W = 160
const H = 80
const CY = H / 2
const CHUCK = 18
const END = W - 8
const SLOPE = 8
const TEETH = 12
const TOOTH = 3

// Sawtooth from (x0, y0) to (x1, y1), roots pushed `inward` (±1, in y).
function teeth(x0: number, y0: number, x1: number, y1: number, inward: number): [number, number][] {
  const pts: [number, number][] = []
  for (let k = 0; k <= TEETH * 2; k++) {
    const t = k / (TEETH * 2)
    pts.push([x0 + (x1 - x0) * t, y0 + (y1 - y0) * t + (k % 2 ? inward * TOOTH : 0)])
  }
  return pts
}

const path = (pts: [number, number][]) => pts.map(([x, y]) => `${x.toFixed(1)},${y.toFixed(1)}`).join(" ")

// Side view, chuck on the left. Slope is exaggerated; only its direction is real.
export function ThreadSketch({ ratio, internal }: { ratio: number; internal: boolean }) {
  const s = Number.isFinite(ratio) ? Math.sign(ratio) * SLOPE : 0
  let body: React.ReactNode
  if (!internal) {
    const r0 = 22 + s / 2
    const r1 = 22 - s / 2
    const top = teeth(CHUCK, CY - r0, END, CY - r1, 1)
    const bottom = teeth(CHUCK, CY + r0, END, CY + r1, -1).reverse()
    body = <polygon points={path([...top, ...bottom])} className="fill-muted stroke-foreground" strokeWidth={1} />
  } else {
    const r0 = 13 - s / 2
    const r1 = 13 + s / 2
    const top = teeth(CHUCK, CY - r0, END, CY - r1, -1)
    const bottom = teeth(CHUCK, CY + r0, END, CY + r1, 1).reverse()
    body = (
      <>
        <rect x={CHUCK} y={8} width={END - CHUCK} height={H - 16} className="fill-muted stroke-foreground" strokeWidth={1} />
        <polygon points={path([...top, ...bottom])} className="fill-card stroke-foreground" strokeWidth={1} />
      </>
    )
  }
  return (
    <figure className="flex flex-col items-center gap-1">
      <svg viewBox={`0 0 ${W} ${H}`} className="h-auto w-full max-w-[200px]" role="img" aria-label={internal ? "Internal thread" : "External thread"}>
        <rect x={0} y={2} width={CHUCK} height={H - 4} rx={2} className="fill-muted-foreground/40" />
        {body}
        <line x1={0} y1={CY} x2={W} y2={CY} className="stroke-muted-foreground" strokeWidth={0.75} strokeDasharray="4 3" />
      </svg>
      <figcaption className="text-xs text-muted-foreground">{internal ? "Internal" : "External"}</figcaption>
    </figure>
  )
}
