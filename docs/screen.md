# Display (Nextion screen)

## Hardware and tooling

- Genuine Nextion Intelligent panel, NX8048P050-011 (5.0", 800x480), closed
  MCU ("Ai HMI T8") -- cannot run custom firmware; stays on stock Nextion
  firmware.
- UI source: nxwright/nxlang in `displays/nextion-5in/`. Built to `.HMI` with nxwright,
  compiled to `.tft` in Nextion Editor (Windows/Wine), pushed over UART by
  the main firmware (`docs/protocol.md`, "Display UI upload").
- Direct `.tft` generation/patching tools were ruled out: they can only patch
  an existing Editor-compiled project, not add/remove/relayout widgets.

## Link with the main firmware

- Firmware is the source of truth. Anything changed on the display shows up
  on the web client and vice versa.
- Display -> firmware: command-set lines (`docs/protocol.md`), through the
  same dispatcher as the web/serial console. Framed as lead byte + `prints`
  text + `FF FF FF`, sent by one nxlang helper. The lead byte is one Nextion
  never starts a return frame with, so commands and Nextion's own frames
  share the stream unambiguously. Commands are readable text, so component
  order/IDs don't matter to the firmware.
- Replies: `ok` is dropped; `err` is shown as a toast on the current page.
- Firmware RX is interrupt-driven: the UART RX ISR fills a ring buffer and
  wakes a Display thread that splits frames on `FF FF FF` and dispatches
  them immediately. Partial frames carry over to the next read. The `.tft`
  upload reads the same ring buffer (or polls with RX IRQ disabled).
- Firmware -> display: attribute updates (`obj.txt=...`, `vis`, ...) driven
  by the same state the web pushes use.
- Display handles page navigation and the keypad locally, sending only
  finished values.
- Each page's load event sends `sendme`; the reply's page id tells the
  firmware to resync and push that page.
- `ui ...` lines are display-local navigation (thread picker tab), handled
  by the Display link, not the command dispatcher.
- Liveness: firmware periodically pings the display (Nextion's `sendme`,
  which replies with the current page). No reply -> controlled stop of any
  running operation or jog (not EStop).
- Each ping also zeroes the display's `linkAge` and sets `linkFlashing`
  (`program.nx` globals). Every page ages the link on a timer and shows a
  non-dismissable blocking toast: blue while flashing, red once the link is
  lost. It starts shown, so it covers boot until the first heartbeat.
- Header icons at the top left of page0 (icons from the Lucide set, drawn
  from SVG files in `pictures/icons`):
  - Estop icon: a tap sends `estop on`. In estop a full-screen cover shows
    a big estop icon. A tap on it sends `estop off`. A line at the bottom
    says to tap to disarm; a refusal replaces it in red.
  - Home icon: a tap sends `home`. Its colour shows the homed state: grey =
    not homed, green = homed, blue = homing in progress. Only axes with a
    home input count.
  - WCS selector: a button with the active work coordinate system. A tap
    opens a list of G54 to G59. A pick sends the G-code word as an MDI line.
    A refusal shows as a toast.
  - The home icon and the WCS selector show only when the pin map has home
    inputs.
- Axis enable buttons send `power on|off z|x`.

## Layout

- Dark theme.
- Always-visible area: Z/X position, axis enable toggles, spindle RPM and
  angle, current mode (tap -> mode picker), start/stop, mm/inch selection,
  jog buttons and step selection.
- Rest of the screen: current mode's page.
- Numeric input: on-screen keypad, computer numpad layout.
- Network page: Wi-Fi enable/disable, force AP mode, connection state and
  IP. Network info appears only on this page.
- Machine settings are web-only.

## Axes

- Enable/disable toggle per axis. Disabled = driver de-energized for manual
  cranking; readout shown as stale (no scales) and kept until re-zeroed.
  Disabling an axis stops any mode using it; a mode can't start while one
  of its axes is disabled; that axis's jog buttons are inactive.
- X shows plain position (no diameter mode).

## Jog

- Tap moves one step; hold moves continuously, accelerating, until release.
- The tap is sent on release. A press longer than the hold delay sends
  only hold commands, so the axis does not stop between tap and hold.
- Hold is sent as repeated jog command lines from a page timer, each
  extending the target just past the stopping distance -- motion ends by
  itself if repeats stop (lost frame, display reset, cable), so no separate
  keepalive is needed for jog.
- Step: presets plus a custom typed value.
- A jog or handwheel turn on an axis a running mode drives stops the mode,
  then moves.
- Jog is not limited by job endpoints.

## Limits and job geometry

- No global travel stops. A homed axis has travel limits: the home
  position and `maxTravelMm` below it. An axis that is not homed has none.
- Each mode defines its own geometry: start = position at engage
  (touch-off) or typed; lengths/depths typed. A [here] helper may fill a
  length from the current position, but is never required.

## Units

- Global mm/inch. TPI is a pitch entry option, not a global unit.

## Modes

What each mode does: `docs/features.md`. Display specifics:

- One page per mode; the firmware lays out the mode's parameter cells.
- Thread: the Library page (tabs per standard, grid of designations) fills
  pitch, depth, taper and flank angle from the thread table. The "custom"
  group starts empty. Flank and Spring are latching buttons. Passes,
  starts, taper and the latches use 3-column rows.
- Probe: a panel, not a mode; `docs/probe.md`.
- A mode that needs a missing feature (X motor, probe) is not in the menu.
  The X column stays; with no X motor it shows X off, the X power button is refused and the
  X jog buttons do nothing.
- The Settings page has a toggle per feature (X motor, Homing, Probe).
- Turn, Thread, Part and Ellipse have a spindle Fwd/Rev toggle.
- Multi-pass modes show phase and pass n/m, and offer skip.
- Every mode but Thread has a feed override row above
  start/stop: step down, the current percentage (tap = 100%, orange when
  not 100%), step up. Active only while running; scales every feed of the
  job, not rapids.
- The `pitch` param shows as "Feed" everywhere but Thread.
- Selected mode and its parameters persist in `RunSettings`.
