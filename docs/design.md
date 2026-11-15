# ScrewSync — Design Notes

## Hardware

- Target: [nanoels](https://github.com/kachurovskiy/nanoels) H5 hardware.
  Two separate boards: a generic ESP32-S3 main controller
  (`esp32-s3-devkitc-1`-class module) and a separate display module, linked
  over UART. Connector assignments: `docs/install.md`.
- The display is a genuine Nextion Intelligent-series panel running
  Nextion's own closed firmware. It cannot run custom firmware -- see
  `docs/screen.md`.
- Axes: Z and X only.

## Firmware split

- Main controller: Zephyr RTOS, in `main/`. Handles motion control,
  encoders, steppers, Wi-Fi and the command protocol. Modules live under
  `main/lib/`, one subdirectory each.
- Display: the panel's own closed Nextion firmware, not custom code. Its UI
  is authored with [nxwright](https://github.com/nixiewright/nxwright)
  (`nxlang` source in `displays/nextion-5in/`, builds to a Nextion `.HMI` project),
  compiled to `.tft` with the official Nextion Editor and pushed to the
  display over UART by the main firmware. See `docs/screen.md`.
- Web client: in `web/`, served by the main firmware, talking to it over a
  WebSocket.
- The display, the web client and the serial console all send command-set
  lines (`docs/protocol.md`) into one dispatcher. Firmware drives the
  display with Nextion attribute-set commands.
- Serial tools in `tools/` pull the command grammar from the device at
  runtime rather than hardcoding it.
- Build tooling: `west` builds and flashes the Zephyr side; the top-level
  `Makefile` drives it, the display build and the release images.

## Boards, displays, targets

- Board (`boards/<board>/`): the controller hardware. `board.overlay` holds
  pins and flash partitions, `board.conf` its Kconfig, `board.mk` its Zephyr
  board and flash addresses.
- Display (`displays/<display>/`): the UI and its firmware code.
  `display.cmake` adds its sources to the build, `display.conf` its Kconfig,
  `display.mk` its build rules. Shared Nextion rules are in
  `displays/nextion.mk`.
- Target (`targets/<target>/`): one board plus one display, and how they
  connect. `target.overlay` sets the display UART (chosen node
  `screwsync,display-uart`) and the display partition. `target.mk` names the
  board and display and calls the templates.
- `targets/template.mk` gives each target `<t>-build`, `-flash`, `-firmware`,
  `-backup`, `-restore`, `-smoke`, `-smoke-interactive`, `-tidy` and the
  region erase rules. A target overrides one by setting `<t>_OWN_<RULE>`.
- The firmware build fails if a target leaves out something the display
  code needs (chosen UART, partition, Kconfig).
- `Makefile.local` (not in git) is optional and holds personal shortcuts.

## Pin map

User side: [pins.md](pins.md).

- Board data: a `screwsync,connectors` node in `board.overlay` (binding
  `main/dts/bindings/screwsync,connectors.yaml`). One child per connector:
  title, row, column and pins. Power pins are `PIN_5V` / `PIN_GND` from
  `main/include/dt-bindings/screwsync/pins.h`. The node also holds the
  strapping pins, the reserved pins with a note each, and the default pins
  of each function in signal order. The default pins of a function must be
  next to each other in one connector.
- `main/lib/Pins` builds the board table from the node with `DT_FOREACH`
  macros. The PCNT unit count is the number of PCNT units in the
  devicetree.
- To add a function: add a row to `FUNCTIONS` in `PinMap.cpp` and the code
  that uses its pins. The web client reads the table from
  `GET /pins/functions`.
- At boot, `Pins` connects each hw encoder's A and B to its PCNT unit
  through the GPIO matrix and sets its glitch filter. The PCNT pins in the
  board's `pinctrl` are only the routing until then. Unused units get a
  constant low input.
- The saved map is keyed by function and connector id, so a new function or
  connector does not break it.
- The firmware (`PinMap::validate`) and the web client (`validate`,
  `nameIssues` in `web/client/pins.ts`) must check the same rules.

## Smoke tests

- `tests/smoke/`, run on real hardware with `<t>-smoke` (automatic) and
  `<t>-smoke-interactive` (operator steps, each can be skipped).
- `run.sh` backs up the settings, display and G-code partitions first and
  always restores them at the end. The backup is skipped when the flash
  still matches the latest one.
- The tests do not build. Build the firmware and the display first.

## Machine state

- `Machine` (firmware module) tracks one derived enum, `Machine::State {
  Idle, Running, Flashing, EStop }`, read via `Machine::state()`. Nothing
  sets this enum directly -- each source announces only its own condition:
  `Machine::setSpinning(bool)` (spindle RPM, from `Rpm`),
  `Machine::setModeEngaged(Axis, bool)` (from `Mode`, one flag per axis it
  drives), and `Machine::setFlashing(bool)` (from `Ota`). `Jog` has no
  engage/disengage of its own, so it has no setter to call.
- `state()` derives on read: `Flashing` if the flashing flag is set >
  `EStop` (sticky, see below) > `Running` if spinning or either axis's mode
  flag is set > else `Idle`.
- The machine always boots in `EStop`. The flag is not saved.
- `Machine::estopNow()` is the one estop entry. It is ISR-safe: it halts
  both axes at once, then a work item sets `EStop`, stops all motion
  (`Mode::stop()`, which also clears the MDI queue) and disables both axes
  (`Stepper::setDisabled`). It does not reboot. `estop on`, the home and
  estop inputs and `setFlashing(true)` while spinning or mode-engaged call
  it -- OTA must never start while the machine is doing something.
- `estop off` calls `Machine::estopOff()`. It is refused while an estop
  input is active. It clears `EStop`, does not
  reboot and does not enable the motors. The axis disabled state is not
  saved, so both axes are disabled at boot. `RunSettings::setAxisDisabled`
  refuses to enable an axis in `EStop`.
- In `EStop`, `Command::apply()` refuses `start`, jog moves, MDI lines and
  `o<...>` calls. Everything else runs: status, settings, pins, Wi-Fi,
  units, mode select and parameters, threads, G-code files, OTA.
- `Stepper::moveTo()` -- the one choke point every mover (`Jog`, modes, the
  G-code step runner) already goes through -- refuses any new target while
  `Flashing`/`EStop`, pinning it to the current position instead. This is
  what actually stops a hand-cranked handwheel: `Jog` has no on/off switch,
  so rejecting commands alone doesn't stop its poll loop from calling
  `moveTo()`; gating `moveTo()` itself does.
- Guards reading `Machine::state()`/`Machine::blocksTraffic()`:
  - `Command::apply()` rejects every command, any transport (console,
    WebSocket, display UART), while state is `Flashing`. In `EStop` it
    rejects only the commands listed above.
  - Each WebUi HTTP resource (`/`, `/wifi`, `/ws`, `/update`,
    `/update-display`) rejects new requests with 503 while `Flashing`.
    `/update` and `/update-display` additionally track which
    client owns the in-flight transfer and reject any other client's
    request with 409 while one is running. Starting either transfer closes
    every open WebSocket in `WsCommand`'s client table and refuses new `/ws`
    upgrades until `Flashing` ends.
  - Display: firmware mirrors `state` to it; it shows a blocking toast
    while `Flashing` or the link is lost, and an `estop off` toast while
    `EStop` (`docs/screen.md`).

## Modes

- `Mode` keeps a table of modes, one file each under `main/lib/Mode/`.
  A mode supplies start/tick hooks and a parameter table; the parameters
  persist in `RunSettings` under the mode's name, and the `<mode>` command,
  status and validation are generic over that table.
- One runner thread reads the spindle once per tick and ticks the running
  mode under the mode lock; every mode-driven move goes through
  `Stepper::moveTo()` with a speed cap (synced and rapid moves use the
  axis's rapid speed).
- A running mode owns the axes it drives. Any jog on an owned axis
  (handwheel, display button, `jog` command) stops the mode, then moves.
- MDI lines run on the G-code step runner when no mode runs, and own both
  axes while they move.
- Positions are in du. `Machine::position()` is the machine coordinate,
  from the stepper count. `Wcs` holds the work offset (G54, RAM only);
  zeroing and the probe set it. A homed axis has a travel window in
  stepper steps: the upper limit at the home position, the lower limit
  `maxTravelMm` below it. `Homing` sets and clears it with the homed state.
- Geometry is relative to the engage point; there are no global stops.
  Directions follow the display jog arrows: -X toward the centerline, -Z
  toward the chuck. Spindle counts increase in forward rotation. Pitches
  and speeds are magnitudes; `dir -|+` (thread: `hand rh|lh`) picks a
  feed toward -Z/-X or +Z/+X; external/internal picks the depth direction.
- Params marked required (pitch/speed, length, depth) at 0 are "not set":
  start is refused until they are, and they can't be zeroed while running.
- Turn, thread and slot share a pass engine (`ModePass`): plunge, cut,
  retract, return to start approaching in the cut direction (backlash taken
  up before the cut).
- Thread mode, G-code threading (G33) and Gear share one helix per pitch: through axis 0 at
  spindle angle 0, offset per start. Passes and reruns land in the same
  groove; Gear holds the axis until the spindle reaches that phase.
- A target the travel clamp cuts short, or a disabled axis, ends the job
  with an error.

## G-code

- `main/lib/Gcode` has no Zephyr dependency and no heap. A `Dialect`
  (`lathecode`, `linuxcnc`, `ngc`) turns its lines into standard
  (LinuxCNC) words. `Interpreter` turns the words into `Step`s.
- `Program` runs a program at execution time: it reads a line only after
  the steps before it are done, so a G38 result is known to the next
  line. It also runs the O-word flow and the routine calls. `GcodeNgc`
  parses the parameters and expressions.
- `Mdi` keeps the modal state between MDI lines. A `@` line runs on a
  fresh interpreter at the session position.
- `ModeSteps` is the one step runner for programs, MDI and routines. It
  drives Z and X together: the axis with the larger delta moves at the
  computed speed, the other follows its progress. The probe interrupt
  halts a G38 move with no decel. Step targets are work coordinates.
- NGC parameters are shared by MDI and jobs and kept until reboot.
- `GcodeStore` holds the saved programs and routines. A saved routine
  replaces the built-in one (`main/lib/GcodeStore/routines/`) with the
  same label.

## Thread table

- Groups (standards) carry depth factors and taper; entries carry
  designation, major diameter and pitch. Built-in defaults:
  `main/lib/Thread/ThreadTable.cpp`.
- `ThreadStore` holds the live table: the defaults until edited on the web,
  then a full copy in NVS (removed defaults stay removed) until reset.

## UI

- No reusing the same buttons/fields for different actions, no custom
  procedures the user has to learn.
- Numeric inputs use an on-screen numeric keyboard with a standard computer
  keyboard layout.
- Display UI design: `docs/screen.md`.

## Licensing

- AGPL v3 or later (`LICENSE`).
- No nanoels code is used, only its hardware.
