// Flavor-neutral G-code pieces, mirroring main/lib/Gcode/GcodeCore.{hpp,cpp}:
// tokenizer, interpreted Step, Flavor interface.

export const DU_PER_MM = 10000
export const DU_PER_INCH = 254000

export const MAX_WORDS = 16
export const MAX_LINE_LEN = 128

export interface Word {
  letter: string
  value: number
}

export type StepKind = "rapid" | "feed" | "thread" | "pause" | "optional-pause" | "end"

// One interpreted motion/control action. zDu/xDu are absolute targets,
// meaningful for the motion kinds only.
export interface Step {
  kind: StepKind
  zDu: number
  xDu: number
  feed: number // du/min for Feed, lead du per spindle rev for Thread
  line: number
}

export interface StepSink {
  emit(step: Step): void
}

// A flavor owns its units, axis conventions and feed semantics.
export interface Flavor {
  begin(startZDu: number, startXDu: number): void
  line(words: Word[], lineNo: number, sink: StepSink): string | null
  id(): string
}

function isSpace(c: string): boolean {
  return c === " " || c === "\t" || c === "\r" || c === "\n"
}

// Plain decimals only; anything after the digits starts the next word.
// Returns null if there are no digits at the given position.
function parseNumber(s: string, pos: number): { value: number; end: number } | null {
  let p = pos
  while (p < s.length && isSpace(s[p])) p++

  let negative = false
  if (s[p] === "+" || s[p] === "-") {
    negative = s[p] === "-"
    p++
  }

  let anyDigits = false
  let intPart = 0
  while (p < s.length && s[p] >= "0" && s[p] <= "9") {
    intPart = intPart * 10 + (s.charCodeAt(p) - 48)
    anyDigits = true
    p++
  }

  let fracPart = 0
  let fracScale = 1
  if (s[p] === ".") {
    p++
    while (p < s.length && s[p] >= "0" && s[p] <= "9") {
      fracScale *= 10
      fracPart = fracPart * 10 + (s.charCodeAt(p) - 48)
      anyDigits = true
      p++
    }
  }

  if (!anyDigits) return null

  let value = intPart + fracPart / fracScale
  if (negative) value = -value
  return { value, end: p }
}

// Splits one source line into words. Strips ';' and '(...)' comments,
// drops N<num> words and lines that are just "%". null on success, else an
// error string.
export function tokenize(text: string): { error: string | null; words: Word[] } {
  let buf = ""
  let inComment = false
  for (let i = 0; i < text.length; i++) {
    const c = text[i]
    if (c === ";") break
    if (c === "(") {
      inComment = true
      continue
    }
    if (c === ")") {
      inComment = false
      continue
    }
    if (inComment) continue
    if (buf.length + 1 >= MAX_LINE_LEN) {
      return { error: "line too long", words: [] }
    }
    buf += c
  }

  let i = 0
  while (i < buf.length && isSpace(buf[i])) i++
  if (i >= buf.length) {
    return { error: null, words: [] }
  }
  if (buf[i] === "%") {
    let k = i + 1
    while (k < buf.length && isSpace(buf[k])) k++
    if (k >= buf.length) {
      return { error: null, words: [] }
    }
  }

  const words: Word[] = []
  while (i < buf.length) {
    while (i < buf.length && isSpace(buf[i])) i++
    if (i >= buf.length) break

    let letter = buf[i]
    if (letter >= "a" && letter <= "z") {
      letter = letter.toUpperCase()
    }
    if (letter < "A" || letter > "Z") {
      return { error: "malformed word", words: [] }
    }
    i++

    const num = parseNumber(buf, i)
    if (!num) {
      return { error: "malformed word", words: [] }
    }
    i = num.end

    if (letter === "N") continue
    if (words.length >= MAX_WORDS) {
      return { error: "too many words", words: [] }
    }
    words.push({ letter, value: num.value })
  }
  return { error: null, words }
}

// Tokenizes text then interprets it with flavor.
export function interpretLine(flavor: Flavor, text: string, lineNo: number, sink: StepSink): string | null {
  const { error, words } = tokenize(text)
  if (error) return error
  if (words.length === 0) return null
  return flavor.line(words, lineNo, sink)
}
