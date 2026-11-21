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
- `ui ...` lines are display-local navigation (thread picker tab, `ui pins
  on|off` for the Pins view, `ui run on|off` for the running view, `ui page
  <n>` for a mode's cell page),
  handled by the Display link, not the command dispatcher.
- Liveness: firmware periodically pings the display (Nextion's `sendme`,
  which replies with the current page). No reply -> controlled stop of any
  running operation or jog (not EStop).
- Each ping also zeroes the display's `linkAge` and sets `linkFlashing`
  (`program.nx` globals). Every page ages the link on a timer and shows a
  non-dismissable blocking toast while flashing or once the link is lost.
  It starts shown, so it covers boot until the first heartbeat.
- Icons are from the Lucide set, drawn from SVG files in `pictures/icons`.
- In estop a full-screen cover blocks the UI until the estop is cleared.
- Homing and the work coordinate selector show only when the pin map has
  home inputs. A coordinate system pick is sent as an MDI line.

## Layout

- Dark theme.
- Machine state, axes and jog are always visible; the rest of the screen
  is the current mode's page.
- Numeric input: on-screen keypad.
- Machine settings are web-only.
- Pins view: the live state of the configured probe, home, e-stop and
  output pins. Information only. The firmware samples and pushes pin states
  only while it is open.

## Axes

- Enable/disable per axis. Disabled = driver de-energized for manual
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
  (touch-off); lengths and depths are typed.

## Units

- Global mm/inch. TPI is a pitch entry option, not a global unit.

## Modes

What each mode does: `docs/features.md`. Display specifics:

- The firmware lays out each mode's parameter cells. A mode can have more
  than one cell page; it opens on the first.
- Thread: the Library page fills pitch, depth, taper and flank angle from
  the thread table. The "custom" group starts empty.
- Probe: a panel, not a mode; `docs/probe.md`.
- A mode that needs a missing feature (X motor, probe) is not in the menu.
  The X column stays; with no X motor it shows X off, the X power button is refused and the
  X jog buttons do nothing.
- Multi-pass modes show pass n/m on the stop button, and offer skip.
- Running view: replaces the cells from Prepare until the job ends. Before
  the start it offers Go to start, Start here and Back. A job started
  elsewhere opens it too. G-code and Probe have none.
- Every mode but Thread has a feed override in the running view. It scales
  every feed of the job, not rapids. G-code shows it on its own view.
- Thread and Gear show the lead-in in the running view. With the spindle
  stopped, a red line asks to start it.
- Selected mode and its parameters persist in `RunSettings`.
