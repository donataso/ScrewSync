// LatheCode's `; Axes expected: Z+ points <left|right>, X+ points <up|down>`
// header (from its AXES directive) must say "right, down" to match our
// hardware (-Z toward the chuck, -X toward the centerline). Mirrors
// main/lib/GcodeStore/GcodeMeta.{hpp,cpp}'s axesMismatch.

const HEADER_RE = /axes expected:\s*z\+\s*points\s*(left|right)\s*,\s*x\+\s*points\s*(up|down)/i

export interface AxesHeader {
  zDir: "left" | "right"
  xDir: "up" | "down"
  start: number // header line's match offset in text
  lineEnd: number // offset of the header line's '\n', or text.length
}

// The header line, if the text has one; null if there's no header (no check).
export function parseAxesHeader(text: string): AxesHeader | null {
  const m = HEADER_RE.exec(text)
  if (!m) return null
  const nl = text.indexOf("\n", m.index)
  return {
    zDir: m[1].toLowerCase() as "left" | "right",
    xDir: m[2].toLowerCase() as "up" | "down",
    start: m.index,
    lineEnd: nl === -1 ? text.length : nl,
  }
}

export function axesMismatched(header: AxesHeader): boolean {
  return header.zDir !== "right" || header.xDir !== "down"
}

// Negates one word's numeric value, preserving its format otherwise: "0"
// stays "0" (no sign added), "-0.1" -> "0.1", "20" -> "-20".
function negateDigits(sign: string, digits: string): string {
  if (/^0+(\.0+)?$/.test(digits)) return digits
  return sign === "-" ? digits : "-" + digits
}

const NUMBER_RE = /^([+-]?)(\d+(?:\.\d+)?)/

// Negates Z and/or X words (per `letters`) on one line, skipping ';' and
// '(...)' comments exactly like the tokenizer (gcode/core.ts tokenize).
function negateAxisWordsInLine(line: string, letters: Set<string>): string {
  let out = ""
  let i = 0
  let inParen = false
  while (i < line.length) {
    const c = line[i]
    if (c === ";") {
      out += line.slice(i)
      break
    }
    if (c === "(") inParen = true
    if (inParen) {
      out += c
      if (c === ")") inParen = false
      i++
      continue
    }
    if (letters.has(c.toUpperCase())) {
      const m = NUMBER_RE.exec(line.slice(i + 1))
      if (m) {
        out += c + negateDigits(m[1], m[2])
        i += 1 + m[0].length
        continue
      }
    }
    out += c
    i++
  }
  return out
}

function negateAxisWords(text: string, letters: Set<string>): string {
  if (letters.size === 0) return text
  return text
    .split("\n")
    .map((line) => negateAxisWordsInLine(line, letters))
    .join("\n")
}

// Rewrites the header to "right, down" and negates the mismatched axis'
// words everywhere else. No-op if there's no header or it already matches.
export function flipMismatchedAxes(text: string): string {
  const header = parseAxesHeader(text)
  if (!header || !axesMismatched(header)) return text

  const letters = new Set<string>()
  if (header.zDir !== "right") letters.add("Z")
  if (header.xDir !== "down") letters.add("X")

  const before = text.slice(0, header.start)
  const headerLine = text
    .slice(header.start, header.lineEnd)
    .replace(/(z\+\s*points\s*)(left|right)/i, "$1right")
    .replace(/(x\+\s*points\s*)(up|down)/i, "$1down")
  const after = text.slice(header.lineEnd)

  return negateAxisWords(before, letters) + headerLine + negateAxisWords(after, letters)
}
