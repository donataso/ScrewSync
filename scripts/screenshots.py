#!/usr/bin/env python
"""Regenerate docs/images screenshots in nxwright's headless simulator.

Run with nxwright's own Python, from anywhere:
    <nxwright>/.venv/bin/python scripts/screenshots.py [shot ...]
No shot names = all shots. Firmware pushes are emulated as main/lib/Display builds them.
"""
import io
import random
import re
import sys
from pathlib import Path

from nxwright.mcp_server.headless import HeadlessSimulation
from PIL import Image, ImageChops, ImageDraw

ROOT = Path(__file__).resolve().parent.parent
OUT = ROOT / "docs" / "images"

_DISPLAY_THEME = (ROOT / "main/lib/Display/DisplayTheme.hpp").read_text()
COLOR = {m[1]: int(m[2]) for m in re.finditer(r"constexpr int COLOR_(\w+) = (\d+);", _DISPLAY_THEME)}
FONT_SYMBOLS = int(re.search(r"constexpr int FONT_SYMBOLS = (\d+);", _DISPLAY_THEME)[1])
_CELL_ENUM = re.search(r"enum class CellType[^{]*\{(.*?)\};", _DISPLAY_THEME, re.S)[1]
KIND = {m[1]: int(m[2]) for m in re.finditer(r"(\w+) = (\d+)", _CELL_ENUM)}

_THREAD_TABLE = (ROOT / "main/lib/Thread/ThreadTable.cpp").read_text()

PREVIEW = {m[1]: int(m[2]) for m in re.finditer(r"constexpr int (PREVIEW_[XYWH]|TOOL_SIZE) = (\d+);", _DISPLAY_THEME)}
_PREVIEW_TS = (ROOT / "web/client/gcode/preview.ts").read_text()


def _scaled(value, scale):
    whole, frac = divmod(value, scale)
    return str(whole) if not frac else ("%d.%0*d" % (whole, len(str(scale)) - 1, frac)).rstrip("0")


def thread_groups():
    defaults = _THREAD_TABLE[_THREAD_TABLE.index("size_t loadDefaults"):]
    groups = []
    for m in re.finditer(r'addGroup\(t, "(\w+)"|_NAMES\[\] = \{([^}]*)\}', defaults):
        groups += [m[1]] if m[1] else re.findall(r'"(\w+)"', m[2])
    return groups


def metric_designations():
    rows = re.search(r"const MetricRow METRIC\[\] = \{(.*?)\n    \};", _THREAD_TABLE, re.S)[1]
    out = []
    for dia, coarse, fine in re.findall(r"\{(\d+), (\d+), \{([\d, ]*)\}\}", rows):
        d = _scaled(int(dia), 1000)
        if int(coarse):
            out.append("M" + d)
        out += ["M%sx%s" % (d, _scaled(int(f), 1000)) for f in re.findall(r"\d+", fine)]
    return out


# Nextion return codes for a refused instruction.
_REFUSED = {0x00, 0x02, 0x03, 0x04, 0x05, 0x06, 0x1A, 0x1B, 0x1C, 0x1D, 0x1E, 0x1F, 0x20, 0x23, 0x24}
_RETURN_CODE = re.compile(r"^sends ([0-9a-f]{2}) ff ff ff", re.I)

SYNC = ["click modeSync,1", "click layoutSync,1"]


def tap(name):
    return ("tap", name)


def angle():
    return 'angle.txt="%d.%d°"' % (random.randrange(360), random.randint(1, 9))


def rpm():
    return 'rpm.txt="%d"' % random.randint(100, 500)


def value(slot, text, kind):
    return ['c%d.txt="%s"' % (slot, text), "cKind%d=%d" % (slot, KIND[kind])]


def toggle(slot, a, b, selected, font=None):
    lines = ['c%da.txt="%s"' % (slot, a), 'c%db.txt="%s"' % (slot, b)]
    if font is not None:
        lines += ["c%da.font=%d" % (slot, font), "c%db.font=%d" % (slot, font)]
    return lines + ["c%d%s.bco=%d" % (slot, "ab"[selected], COLOR["SELECTED"]),
                    "cKind%d=%d" % (slot, KIND["Toggle"])]


def latch(slot, label):
    return ['c%d.txt="%s"' % (slot, label),
            "c%d.bco=%d" % (slot, COLOR["SELECTED"]),
            "c%d.borderc=%d" % (slot, COLOR["SELECTED"]),
            "cKind%d=%d" % (slot, KIND["Latch"])]


def common():
    return ["estop.val=0", "click estopSync,1",
            "zOn.val=1", "xOn.val=1", 'zPos.txt="12.480"', 'xPos.txt="0.650"', angle()]


HOMED = ["homeState.val=1", 'wcsBtn.txt="G54"', "wcs0.bco=%d" % COLOR["SELECTED"], "click layoutSync,1"]


MODES = ["Gear", "Async", "Turn", "Thread", "Part", "Slot", "Ellipse", "G-code", "Probe"]
MODE_CMDS = ["gear", "async", "turn", "thread", "part", "slot", "ellipse", "gcode", "probe"]
MENU = (['menu%d.txt="%s"' % (i, m) for i, m in enumerate(MODES)]
        + ['menuCmd%d.txt="%s"' % (i, m) for i, m in enumerate(MODE_CMDS)]
        + ['modeName.txt="THREAD"', "menu3.bco=%d" % COLOR["SELECTED"],
           "menu%d.bco=%d" % (MODES.index("Probe"), COLOR["SETTINGS"]), "click modeSync,1"])

IDLE = ["ready.val=1", "prepares.val=1", 'startStop.txt="PREPARE"']

THREAD = (['modeName.txt="THREAD"']
          + value(0, "Pitch: 1.75 mm", "Pitch")
          + toggle(1, "LH", "RH", 1)
          + value(2, "Depth: 1.074 mm", "Length")
          + toggle(3, "Ext", "Int", 0)
          + value(4, "Length: 20 mm", "Length")
          + toggle(5, "Fwd", "Rev", 0)
          + value(10, "Passes: 8", "Count")
          + value(11, "Starts: 1", "Count")
          + value(12, "Taper: 0°", "Angle")
          + latch(13, "Flank")
          + latch(14, "Spring")
          + ['picker.txt="Library"', "cKind15=%d" % KIND["Picker"]]
          + IDLE
          + SYNC)

DIR_AXIS = (toggle(1, "←", "→", 0, FONT_SYMBOLS)
            + toggle(2, "Z", "X", 0))

OVERRIDE = ['ovrMinus.txt="-10%"', 'ovrPlus.txt="+10%"', "ovrOn.val=1"]

GEAR = ['modeName.txt="GEAR"'] + DIR_AXIS + value(4, "Taper: 0°", "Angle")

X_DISABLED = ["xOn.val=0", "xPos.pco=%d" % COLOR["DIM"]] + [
    line
    for part in ("xZero", "xEnable", "xMinus", "xPlus")
    for line in ("%s.bco=%d" % (part, COLOR["AXIS_OFF"]),
                 "%s.bco2=%d" % (part, COLOR["AXIS_OFF"]),
                 "%s.pco=%d" % (part, COLOR["DIM"]))
]

RUNNING = ["running.val=1", "ready.val=1"]
RUN_VIEW = ["runView.val=2"]


def prepared(lead_in, spindle):
    return (["runView.val=1", "leadInOn.val=1", 'leadIn.txt="Lead-in: %s"' % lead_in,
             "leadIn.pco=%d" % COLOR["TEXT" if spindle else "ERROR"], 'skip.txt="BACK"',
             "goStart.bco=%d" % COLOR["START"]]
            + ([rpm()] if spindle else []))


def picker(group, selected):
    lines = []
    for i, name in enumerate(thread_groups()):
        label = name.upper() if len(name) <= 4 else name.capitalize()
        lines += ['tab%d.txt="%s"' % (i, label), 'tabCmd%d.txt="%s"' % (i, name)]
        if name == group:
            lines.append("tab%d.bco=%d" % (i, COLOR["SELECTED"]))
    for i, d in enumerate(metric_designations()):
        lines += ['g%d.txt="%s"' % (i, re.sub(r"(?<=\d)x(?=\d)", "×", d)), 'gCmd%d.txt="%s"' % (i, d)]
        if d == selected:
            lines.append("g%d.bco=%d" % (i, COLOR["SELECTED"]))
    return lines + ['pickerGroup.txt="%s"' % group, "click pickerSync,1"]


def _preview_color(name):
    r, g, b, *alpha = (float(v) for v in re.search(r'%s = "rgba?\(([^)]*)\)"' % name, _PREVIEW_TS)[1].split(","))
    if not alpha:
        return int(r), int(g), int(b)
    back = _preview_color("BACKGROUND")
    return tuple(round(c * alpha[0] + bc * (1 - alpha[0])) for c, bc in zip((r, g, b), back))


# A roughing and threading program on stock of radius 10, as (kind, Z mm,
# X mm): X is radius, Z0 is the stock's right edge.
def toolpath():
    steps = [("rapid", 2, 0), ("rapid", 2, 10.5)]
    for x in (9.5, 9, 8.5, 8, 7.5, 7, 6.5, 6):
        z_end = -30 if x >= 8 else -18
        steps += [("rapid", 2, x), ("feed", z_end, x), ("feed", z_end, x + 0.5), ("rapid", 2, x + 0.5)]
    for depth in (0.3, 0.55, 0.75, 0.9):
        steps += [("rapid", 2, 6 - depth), ("thread", -15, 6 - depth), ("rapid", -15, 7), ("rapid", 2, 7)]
    return steps


# Equal-scale fit of every step and the start point, as computeMapping in preview.ts.
def preview_mapping(steps, margin=16):
    zs = [0] + [s[1] for s in steps]
    xs = [0] + [s[2] for s in steps]
    w, h = PREVIEW["PREVIEW_W"], PREVIEW["PREVIEW_H"]
    scale = min((w - 2 * margin) / (max(zs) - min(zs)), (h - 2 * margin) / (max(xs) - min(xs)))
    origin_x = w / 2 - (min(zs) + max(zs)) / 2 * scale
    origin_y = h / 2 - (min(xs) + max(xs)) / 2 * scale
    return origin_x, origin_y, scale, min(zs), max(zs)


def _dashed(draw, a, b, fill, on=4, off=3):
    length = ((b[0] - a[0]) ** 2 + (b[1] - a[1]) ** 2) ** 0.5
    t = 0
    while t < length:
        u, v = t / length, min(t + on, length) / length
        draw.line([(a[0] + (b[0] - a[0]) * u, a[1] + (b[1] - a[1]) * u),
                   (a[0] + (b[0] - a[0]) * v, a[1] + (b[1] - a[1]) * v)], fill=fill, width=2)
        t += on + off


# drawPreview in preview.ts, for the picture the web client sends to the display.
def preview_picture(steps):
    origin_x, origin_y, scale, z_min, z_max = preview_mapping(steps)
    px = lambda z, x: (origin_x + z * scale, origin_y + x * scale)
    img = Image.new("RGB", (PREVIEW["PREVIEW_W"], PREVIEW["PREVIEW_H"]), _preview_color("BACKGROUND"))
    draw = ImageDraw.Draw(img)
    draw.line([px(z_min, 0), px(z_max, 0)], fill=_preview_color("COLOR_CENTERLINE"), width=2)
    at = px(0, 0)
    for kind, z, x in steps:
        to = px(z, x)
        if kind == "rapid":
            _dashed(draw, at, to, _preview_color("COLOR_RAPID"))
        else:
            draw.line([at, to], fill=_preview_color("COLOR_THREAD" if kind == "thread" else "COLOR_FEED"), width=2)
        at = to
    sx, sy = px(0, 0)
    draw.ellipse([sx - 3, sy - 3, sx + 3, sy + 3], fill=_preview_color("COLOR_START"))
    return img


# The display draws the picture from its RAM, which the headless simulator
# cannot hold: paste it over the empty picture box, keeping what is drawn on top.
def paste_preview(png, steps):
    shot = Image.open(io.BytesIO(png)).convert("RGB")
    box = (PREVIEW["PREVIEW_X"], PREVIEW["PREVIEW_Y"],
           PREVIEW["PREVIEW_X"] + PREVIEW["PREVIEW_W"], PREVIEW["PREVIEW_Y"] + PREVIEW["PREVIEW_H"])
    region = shot.crop(box)
    empty = max(region.getcolors(region.width * region.height))[1]
    r, g, b = ImageChops.difference(region, Image.new("RGB", region.size, empty)).split()
    mask = ImageChops.lighter(ImageChops.lighter(r, g), b).point(lambda v: 255 if v == 0 else 0)
    region.paste(preview_picture(steps), (0, 0), mask)
    shot.paste(region, box[:2])
    out = io.BytesIO()
    shot.save(out, "PNG")
    return out.getvalue()


def tool_marker(steps, z, x):
    origin_x, origin_y, scale, _, _ = preview_mapping(steps)
    half = PREVIEW["TOOL_SIZE"] // 2
    left = PREVIEW["PREVIEW_X"] + round(origin_x + z * scale) - half
    top = PREVIEW["PREVIEW_Y"] + round(origin_y + x * scale) - half
    return ["gcodeTool.x=%d" % left, "gcodeTool.y=%d" % top, "gcodeToolOn.val=1"]


GCODE_STEPS = toolpath()
GCODE_AT = 24

GCODE = (['modeName.txt="G-CODE"', "gcodeOn.val=1", "gcodeLoaded.val=1", "picOn.val=1",
          'gcodeName.txt="shaft-m12.ngc"',
          'gcodeProgress.txt="%d/%d steps"' % (GCODE_AT, len(GCODE_STEPS)),
          'ovrMinus.txt="-10%"', 'ovrPlus.txt="+10%"', 'ovr.txt="100%"', "ovrOn.val=1"]
         + tool_marker(GCODE_STEPS, -11, GCODE_STEPS[GCODE_AT - 1][2]))

PAGES = {"thread-picker": "threads"}
OVERLAYS = {"gcode-running": lambda png: paste_preview(png, GCODE_STEPS)}


def shots():
    return {
        "thread-m12-idle": common() + THREAD,
        "thread-prepared": common() + THREAD + ["needZero.val=1"] + prepared("6.4 mm", True) + SYNC,
        "thread-prepared-no-spindle": common() + THREAD + ["needZero.val=1"]
        + prepared("start spindle to get the reading", False) + SYNC,
        "thread-picker": picker("metric", "M12"),
        "mode-picker": common() + MENU + [tap("mode")],
        "settings": common() + MENU + ['wifiState.txt="Workshop"', 'wifiIp.txt="192.168.1.42"',
                                       tap("mode"), tap("menuSettings")],
        "gear-running-override": common() + GEAR + X_DISABLED + RUNNING + RUN_VIEW + OVERRIDE
        + value(0, "Feed: 0.05 mm", "Pitch")
        + value(3, "Stop: 50 mm", "Length")
        + [rpm(), 'ovr.txt="130%"', "ovr.bco=%d" % COLOR["ZERO"], "leadInOn.val=1", 'leadIn.txt="Lead-in: 0.1 mm"',
           'startStop.txt="STOP"', "startStop.bco=%d" % COLOR["STOP"]]
        + SYNC,
        "turn-running-mid-pass": common() + ['modeName.txt="TURN"'] + DIR_AXIS + RUNNING + RUN_VIEW + OVERRIDE
        + value(0, "Feed: 0.1 mm/rev", "Length")
        + toggle(3, "Ext", "Int", 0)
        + value(4, "Length: 30 mm", "Length")
        + value(5, "Depth: 2 mm", "Length")
        + value(6, "Passes: 8", "Count")
        + toggle(7, "Fwd", "Rev", 0)
        + [rpm(), 'zPos.txt="-14.350"', 'xPos.txt="-1.000"', 'ovr.txt="100%"',
           'stopPass.txt="STOP 4/8"', "canSkip.val=1"]
        + SYNC,
        "probe-contact": common() + ['modeName.txt="PROBE"', "probeOn.val=1"] + RUNNING
        + value(0, "Distance: 20 mm", "Length")
        + value(1, "Offset: 5 mm", "Length")
        + ['zPos.txt="-3.217"', 'probePhase.txt="back off"',
           "probeLight.bco=%d" % COLOR["START"],
           'startStop.txt="STOP"', "startStop.bco=%d" % COLOR["STOP"]]
        + SYNC,
        "gear-feed-keypad": common() + GEAR
        + value(0, "Feed: 0 mm", "Pitch")
        + value(3, "Stop: 100 mm", "Length")
        + ["c0.pco=%d" % COLOR["NOT_SET"], 'cCmd0.txt="gear pitch "', "prepares.val=1",
           'startStop.txt="PREPARE"', "startStop.pco=%d" % COLOR["DIM"], "startStop.bco=%d" % COLOR["AXIS_OFF"]]
        + SYNC
        + [tap("c0"), tap("kp0"), tap("kpDot"), tap("kp0"), tap("kp7")],
        "gcode-running": common() + HOMED + GCODE + RUNNING
        + [rpm(), 'zPos.txt="-11.000"', 'xPos.txt="%.3f"' % GCODE_STEPS[GCODE_AT - 1][2],
           'startStop.txt="STOP"', "startStop.bco=%d" % COLOR["STOP"]]
        + SYNC,
        "wcs-picker": common() + THREAD + HOMED
        + ['wcsBtn.txt="G55"', "wcs0.bco=%d" % COLOR["BUTTON"], "wcs1.bco=%d" % COLOR["SELECTED"],
           tap("wcsBtn")],
    }


def send(sim, line):
    seen = len(sim.log())
    sim.send(line)
    for entry in sim.log(seen):
        code = _RETURN_CODE.match(entry["line"])
        if code and int(code[1], 16) in _REFUSED:
            raise SystemExit("refused: %s (%s)" % (line, entry["line"]))


def press(sim, name):
    for c in sim.components():
        if c["name"] == name and c["shown"]:
            sim.press(c["x"] + c["w"] // 2, c["y"] + c["h"] // 2)
            sim.release()
            return
    raise SystemExit("cannot tap %s: not shown" % name)


def take(name, steps):
    sim = HeadlessSimulation(ROOT / "displays/nextion-5in", page=PAGES.get(name, "page0"))
    try:
        for step in steps:
            if isinstance(step, tuple):
                press(sim, step[1])
            else:
                send(sim, step)
        path = OUT / ("%s.png" % name)
        png = sim.screen()["png"]
        path.write_bytes(OVERLAYS[name](png) if name in OVERLAYS else png)
        print(path.relative_to(ROOT))
    finally:
        sim.stop()


def main():
    all_shots = shots()
    names = sys.argv[1:] or list(all_shots)
    unknown = [n for n in names if n not in all_shots]
    if unknown:
        raise SystemExit("unknown shot: %s (known: %s)" % (", ".join(unknown), ", ".join(all_shots)))
    for name in names:
        take(name, all_shots[name])


if __name__ == "__main__":
    main()
