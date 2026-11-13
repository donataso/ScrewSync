import { DU_PER_MM, type Step } from "./core"

// Must match the "gcodePic" component's w/h in displays/nextion-5in/pages/page0.page.toml.
export const CANVAS_W = 464
export const CANVAS_H = 288

// The display page's background colour (RGB565 2178).
export const BACKGROUND = "rgb(8, 16, 16)"

const MARGIN_PX = 16
const MIN_SPAN_MM = 1
const DEFAULT_SCALE_PX_PER_MM = 10

const COLOR_CENTERLINE = "rgba(255, 255, 255, 0.18)"
const COLOR_RAPID = "rgb(220, 100, 255)"
const COLOR_FEED = "rgb(150, 220, 255)"
const COLOR_THREAD = "rgb(255, 170, 60)"
const COLOR_START = "rgb(70, 220, 110)"

function duToMm(du: number): number {
  return du / DU_PER_MM
}

export interface ToPx {
  (zMm: number, xMm: number): [number, number]
}

export interface Mapping {
  toPx: ToPx
  // Image px = origin + mm * pxPerMm.
  originX: number
  originY: number
  pxPerMm: number
  zMinMm: number
  zMaxMm: number
  xMinMm: number
  xMaxMm: number
}

// Equal-scale mm -> px mapping that fits every step's target (plus the
// start point) into the canvas with a margin. Z runs left->right, X runs
// top->bottom (X+ down, centerline on top).
export function computeMapping(steps: Step[]): Mapping {
  let zMin = 0
  let zMax = 0
  let xMin = 0
  let xMax = 0
  for (const s of steps) {
    if (s.kind !== "rapid" && s.kind !== "feed" && s.kind !== "thread") {
continue
}
    const zMm = duToMm(s.zDu)
    const xMm = duToMm(s.xDu)
    zMin = Math.min(zMin, zMm)
    zMax = Math.max(zMax, zMm)
    xMin = Math.min(xMin, xMm)
    xMax = Math.max(xMax, xMm)
  }

  const spanZ = Math.max(zMax - zMin, MIN_SPAN_MM)
  const spanX = Math.max(xMax - xMin, MIN_SPAN_MM)
  const availW = CANVAS_W - 2 * MARGIN_PX
  const availH = CANVAS_H - 2 * MARGIN_PX
  const scale = zMax === zMin && xMax === xMin ? DEFAULT_SCALE_PX_PER_MM : Math.min(availW / spanZ, availH / spanX)

  const centerZ = (zMin + zMax) / 2
  const centerX = (xMin + xMax) / 2
  const toPx: ToPx = (zMm, xMm) => {
    const px = CANVAS_W / 2 + (zMm - centerZ) * scale
    const py = CANVAS_H / 2 + (xMm - centerX) * scale
    return [px, py]
  }

  return {
    toPx,
    originX: CANVAS_W / 2 - centerZ * scale,
    originY: CANVAS_H / 2 - centerX * scale,
    pxPerMm: scale,
    zMinMm: zMin,
    zMaxMm: zMax,
    xMinMm: xMin,
    xMaxMm: xMax,
  }
}

// Draws the toolpath onto ctx (a CANVAS_W x CANVAS_H context) and returns
// the mapping used, so a later phase can draw the live tool position with
// the same mm -> px function.
export function drawPreview(ctx: CanvasRenderingContext2D, steps: Step[]): Mapping {
  const mapping = computeMapping(steps)
  const { toPx } = mapping

  ctx.clearRect(0, 0, CANVAS_W, CANVAS_H)
  ctx.fillStyle = BACKGROUND
  ctx.fillRect(0, 0, CANVAS_W, CANVAS_H)

  const [cx0] = toPx(mapping.zMinMm, 0)
  const [cx1] = toPx(mapping.zMaxMm, 0)
  if (mapping.xMinMm <= 0 && mapping.xMaxMm >= 0) {
    const [, py] = toPx(0, 0)
    ctx.save()
    ctx.strokeStyle = COLOR_CENTERLINE
    ctx.lineWidth = 1.5
    ctx.setLineDash([])
    ctx.beginPath()
    ctx.moveTo(Math.min(cx0, cx1), py)
    ctx.lineTo(Math.max(cx0, cx1), py)
    ctx.stroke()
    ctx.restore()
  }

  let zMm = 0
  let xMm = 0
  for (const s of steps) {
    if (s.kind !== "rapid" && s.kind !== "feed" && s.kind !== "thread") {
continue
}
    const nextZMm = duToMm(s.zDu)
    const nextXMm = duToMm(s.xDu)
    const [x0, y0] = toPx(zMm, xMm)
    const [x1, y1] = toPx(nextZMm, nextXMm)

    ctx.save()
    ctx.lineWidth = 1.5
    if (s.kind === "rapid") {
      ctx.strokeStyle = COLOR_RAPID
      ctx.setLineDash([4, 3])
    } else if (s.kind === "thread") {
      ctx.strokeStyle = COLOR_THREAD
      ctx.setLineDash([])
    } else {
      ctx.strokeStyle = COLOR_FEED
      ctx.setLineDash([])
    }
    ctx.beginPath()
    ctx.moveTo(x0, y0)
    ctx.lineTo(x1, y1)
    ctx.stroke()
    ctx.restore()

    zMm = nextZMm
    xMm = nextXMm
  }

  const [startPx, startPy] = toPx(0, 0)
  ctx.fillStyle = COLOR_START
  ctx.beginPath()
  ctx.arc(startPx, startPy, 3, 0, Math.PI * 2)
  ctx.fill()

  return mapping
}

// Exports canvas as a JPEG blob for /display-preview, retrying at lower
// quality if it's over maxBytes. Returns null if even the lowest quality
// doesn't fit.
export async function canvasToJpeg(canvas: HTMLCanvasElement, maxBytes = 64 * 1024): Promise<Blob | null> {
  const qualities = [0.85, 0.7, 0.6, 0.5]
  for (const quality of qualities) {
    const blob = await new Promise<Blob | null>((resolve) => canvas.toBlob(resolve, "image/jpeg", quality))
    if (blob && blob.size <= maxBytes) {
return blob
}
  }
  return null
}
