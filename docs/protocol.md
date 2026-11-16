# ScrewSync — Command Protocol

Shared command set used by `main/` <-> `web/`, the display and the
serial console. One command dispatcher in `main/`, fed by thin transport
adapters (serial shell, WebSocket to `web/`, display UART). Firmware ->
display output is Nextion attribute-set commands, not this protocol's
replies; see `docs/screen.md`.

## Framing

- Serial console: line-oriented text, one command per line (`\n`-terminated),
  human-typeable over serial.
- Display (`main/lib/Display`): lead byte + command line + `FF FF FF`,
  sharing the stream with Nextion's own `FF FF FF`-terminated return
  frames; the lead byte is one Nextion never starts a return frame with.
  `ok` replies are dropped, `err` is shown as a toast. See `docs/screen.md`.
- Web (WebSocket, `main/lib/WebUi/WsCommand.cpp`): JSON. Request
  `{"type":"cmd","line":"<verb> [args...]","id":<n>}`. `id` (a non-negative
  integer) is optional. With an `id`, the client gets exactly one message
  with the same `id`: `{"type":"error","msg":"<reason>"}` on a failure;
  the query's own message (`settings`, `wifi` for `wifi status`, `cal`,
  `display`, `commands`, `ws`) for a query; else
  `{"type":"reply","msg":"<text after ok>"}`. Changes are still broadcast
  as the typed pushes below, without an `id`. Without an `id`, a failure
  gets the `error` message and a success gets no reply of its own.
- All JSON on the wire -- either direction -- is built with
  `zephyr/data/json.h` (`json_obj_encode_buf`/descriptors), never by hand
  with `snprintf`/string concatenation.

## Requests

- `<verb> [args...]` (e.g. `mode thread`, `jog step next`), or a G-code
  line (see [MDI](#mdi)).
- `settings set-all <json>`: applies every field present in one JSON object
  (bulk import, or a settings form saving several fields at once), through
  the same per-field validation as `settings set <field> <value>`, instead
  of one round trip per field.
- Speed settings are in du/min (field names with `DuPerMin`). Acceleration
  settings are in du/s² (`DuPerSec2`). The firmware computes the step
  rates from them and the axis `motorSteps` and `screwPitchDu`.
- Homing settings, per axis (suffix `Z` or `X`): `homeFastDuPerMin`,
  `homeSlowDuPerMin` (du/min), `homeLatchBackOffDu`, `homePullOffDu`,
  `homePositionDu` (du). Global: `requireHoming` (bool).
- Feature switches (bool): `xMotorEnabled`, `homingEnabled`, `probeEnabled`.
  A capability is its switch on and its function wired. A change applies
  after a reboot. Saving settings removes the functions of a feature that
  is off from the saved pin map.
- A switch that is missing from the saved settings, from a `settings reset`
  or from a backup is set at load: `xMotorEnabled` on; `homingEnabled` and
  `probeEnabled` on if their function is wired, else off. In a backup that
  has `pins`, the wiring is read from those pins, else from the saved pin map.
- A command that needs a missing capability gets `err <feature> disabled`
  (switch off) or `err <feature> not wired` (switch on, no function), with
  `<feature>` = `X motor`, `homing` or `probe`. This covers `power on x`,
  X jogs, X moves in MDI, `home`, and the modes that need X (`turn`,
  `thread`, `part`, `slot`, `ellipse`) or the probe (`probe`).
- At boot, a saved mode that needs a missing capability is replaced by
  `gear`.
- `bootState`: the state at power-on. `0` estop, `1` estop off with motors
  off, `2` estop off with motors on. Other restarts and an active estop
  input give estop.
- Serial/UART replies are `ok [data]` / `err <reason>`.
- Any command on any transport gets `err busy: flashing` instead of
  running while machine state is `flashing`.
- While machine state is `estop`, `start`, `home`, `jog`, MDI lines and `o<...>`
  calls get `err busy: estop`. All other commands run.
- `jog step [<deci-microns>|next]`: shows or sets the jog step. `next`
  selects the next preset, then the custom step, then the first preset.
- `jog z|x speed <du/min|0>`: sets the top speed of jogs on that axis. 0
  clears it, and the rapid speed setting applies again.
- `estop` / `estop on` / `estop off`: `estop` alone replies
  `ok state=<name>`. `estop on` trips the EStop. `estop off` clears it. It
  does not enable the motors. While `flashing`, `estop on` gets
  `err busy: flashing`. While an estop input is active, `estop off` gets
  `err busy: estop input active`.
- `home`: homes X (if the X motor exists), then Z. It is refused unless the
  machine is idle, without the homing capability, without a home input
  (`err no home switch`) and while an
  axis is off. `stop` ends it. A failure shows in `error` and the axis is not
  homed.
- `unhome`: clears the homed state of both axes. Refused unless the machine
  is idle.
- While `requireHoming` is on, with home switches, and an axis is not homed:
  `start`, `jog`, `o<...>` calls and MDI lines with a move get `err not homed`.
  G-code that does not move (G54–G59, `G10 L20`) runs.
- `power on|off [z|x]`: turns the motors on or off, both axes without an
  axis. `power on` gets `err busy: estop` in estop. `power on x` gets the
  X motor error when the X motor capability is missing; the `status` push
  then has `enabled` false for X.

## MDI

- A line that starts with G, M, T, F, X or Z, then a digit, sign or dot,
  is G-code (LinuxCNC dialect), e.g. `G91 G0 Z-1`. Reply: `ok` when queued,
  else `err <reason>`.
- The modal state (G90/G91, G20/G21, F, motion mode) stays between lines.
  When the queue is empty, the next line starts from the current position.
- `@` before a line marks a UI line (display, switches, web buttons). It
  runs from a fresh modal state and does not change the MDI one, so it
  gives every mode it needs, e.g. `@G21 G91 G0 Z1`.
- Lines queue while a move runs; a full queue gives `err queue full`.
  Refused while a mode runs. `stop` stops both axes and clears the queue.
- MDI owns both axes while it moves; a jog stops it.
- G0 runs at the rapid speed setting of the axis. G0/G1 move with the
  spindle stopped. The feed override applies to G1.
- `G10 L20 P<n> Z… X…`: the current position reads the given values in
  system n (X is radius). P0 is the active system, P1–P6 are G54–G59.
  Also in programs.
- `G54`–`G59` select the work coordinate system. G54 is active at boot.
  Selecting is allowed at any time. Also in programs.
- `G53 G0|G1 Z… X…`: a move in machine coordinates. It is not modal and
  needs G90. Also in programs.
- With home switches, the offsets of all systems are saved. Without, only
  G54 exists, G55–G59 and `G10 L20 P2`–`P6` get `err G54 only without home
  switches`, and the offset is lost at reboot.
- `G38.2`–`G38.5 <axis>… F…` moves one axis until the probe
  input changes: `.2`/`.3` until contact, `.4`/`.5` until the contact
  opens. `.2`/`.4` give an error if it does not change before the target;
  `.3`/`.5` do not. The axis halts with no deceleration. Not modal. In
  MDI, no line queues behind it. The result is a push:
  `probe z|x found=0|1 pos=<work du> motor=<du>` on the serial console,
  `{"type":"probe","msg":"z|x found=… pos=… motor=…"}` on the WebSocket.
  `motor` is the motor position without backlash compensation.
- MDI only: `M110 [R<deg>]` makes the current spindle angle R (0 if
  omitted).
- Parameters and expressions as in NGC (see "NGC"), evaluated when the
  line is queued. O-words are refused.

## NGC

- LinuxCNC NGC syntax in MDI, programs and routines:
  - `#1`–`#30`: local to each call level; a call sets them from its
    arguments. `#<name>`: local to the call level. `#<_name>`: global.
  - `#5061` (X), `#5063` (Z), `#5070` (1 = tripped): the last G38 move,
    in program units and work coordinates. `#<_probe_motor>`: the probed
    axis's motor position there, without backlash compensation (not
    LinuxCNC). Read-only.
  - Each settings field reads as `#<_name>`: snake case, with `Du`,
    `DuPerMin` and `DuPerSec2` dropped (`probeFeedDuPerMin` is
    `#<_probe_feed>`).
    Those values read in program units. `#<_probe_contact>`: the live probe
    input. `#<_has_x>` and `#<_has_probe>`: 1 or 0, the X motor and probe
    capabilities (same as `has_x_motor` and `has_probe`). All read-only.
  - Parameters keep their values until reboot. MDI and jobs share them.
  - `[...]` expressions: `**`, `* / MOD`, `+ -`, `EQ NE GT GE LT LE`,
    `AND OR XOR`, in that precedence; `ABS ACOS ASIN COS EXP FIX FUP ROUND
    LN SIN SQRT TAN [x]` and `ATAN[y]/[x]`, in degrees.
  - Assignments on a line apply after its words.
  - O-words: `sub`/`endsub`/`call`, `if`/`else`/`endif`,
    `while`/`endwhile`, `repeat`/`endrepeat`. Labels are numbers or
    `<names>`.
  - `(MSG, text)`: the job phase. `(ABORT, text)`: ends the job with
    `text` as the error. `(PRINT, text)`: each `#n` / `#<name>` in `text`
    is replaced by its value; the line is a push, `print <text>` on the
    serial console, `{"type":"print","msg":"<text>"}` on the WebSocket.
    Jobs only.
- Lines run at execution time: a line is read after the moves before it
  end, so a G38 result is known to the next line.
- `o<name> call` finds a sub in the same file, then in the program, then
  a routine (see "G-code (HTTP)"), then a built-in routine
  (`main/lib/GcodeStore/routines/`). A routine shares the modal state of
  its caller.
- `o<name> call [<arg>]...` as a command runs that routine as a G-code
  job, whatever the selected mode. Refused while a job or MDI runs. Its
  G0/G1 moves do not wait for the spindle. Claims only the enabled axes.
  Without the X motor capability, a job is refused before any move if
  the program text or the call line has an X word (outside comments and
  `<names>`, any case, moving or not). Stored routines are not checked by
  text. A dry run of the job, with `#<_has_x>` at 0, refuses an X move in
  a routine that is not guarded; a guarded part is skipped. A move of X
  that a run-time value causes stops the job at that move. A job with a
  G38 move is refused without the probe capability.
- The built-in `o<zero>` skips its X move without the X motor capability.
- Built-in routines (axis 0 = Z, 1 = X; dir -1 or 1):
  - `o<probe> call [<axis>] [<dir>]`: the touch-off cycle; see
    `docs/probe.md`.
  - `o<zero> call [<z>] [<x>]`: rapid to work zero on each axis given 1,
    Z first. The display's go-to-zero, the go-to-zero switches and
    `start zero` use it.
  - `o<backlash> call [<axis>] [<dir>] [<runs>]`: see
    `docs/calibration.md`. Prints `backlash <mm>`.

## Modes

- `mode [<name>]` selects (refused while running) or reports mode, run
  state, phase, pass and the last job error. `start [zero]` (`zero`: the routine
  `o<zero>` first, then the mode starts where it ends), `stop`, `skip` (next
  depth pass of a multi-pass mode).
- `override [status] | + | - | reset`: feed override of the running job
  or MDI move, percent of the set feed, clamped to `Mode::OVERRIDE_*_PCT`.
  Refused when nothing runs or in thread; back to 100 on each `start` and
  when MDI starts from an empty queue.
- Parameters, one verb per mode: `<mode> [status]` lists `name=value ...`;
  `<mode> <param> <value>` sets one. Lengths/pitches in deci-microns,
  speeds in deci-microns per minute, tapers as half-angle degrees (+ =
  diameter grows toward the chuck, regardless of feed direction), `axis
  z|x`, `dir -|+`, `hand rh|lh`, `spindle fwd|rev`, flags `on|off`. Only
  feed-rate params may change while running.
- Spindle-fed modes (turn, thread, part, ellipse) cut in the `spindle`
  rotation and stop with `wrong spindle direction` on the other.
- `thread preset <group> <designation>` fills pitch, depth, taper and
  `angle` (included flank angle) from the thread table. `flank on`:
  alternating flank infeed, each pass shifted along Z to slide down the
  flank the previous pass cut. `spring on`: one extra pass at full depth.
- `threads groups | list <group> [<offset>] | reset`: thread table names,
  paged designations (`next` = offset to continue from), back to the
  built-in defaults.
- `probe` is a display panel, not a mode: it has no params, and `start`
  is refused.

## Calibration

- `cal count`: the raw spindle encoder count. See `docs/calibration.md`.
  Over the WebSocket the reply is `{"type":"cal","msg":"<reply>"}`.

## Thread table (HTTP)

- `GET /threads`: the whole table, `{"groups":[{name, depthExternal,
  depthInternal, taper, flankAngle}], "entries":[{group, designation, majorDu,
  pitchDu}], "overrides":[{group, designation, depthExternalDu,
  depthInternalDu, hasTaper, taper}]}`. Group depths are factors of the
  pitch; taper is diameter change per length, + = growing toward the chuck
  (external; internal flips it). `overrides` lists only entries with their
  own depth (0 = group's) or taper (`hasTaper`).
- `POST /threads`: replaces the whole table (same JSON, `overrides`
  optional). Validated as a whole, then stored in full; replies `ok` or the
  reason. Names and designations may not contain spaces or commas.

## Pin map (HTTP)

- `GET /pins`: the board and the saved map.
- `GET /pins/functions`: the function table. Each function has `features`:
  the features it belongs to (`xMotor`, `homing`, `probe`). The pin editor
  hides the functions of a feature that is off.
- `POST /pins`: validates and saves a map. Refused unless idle. Applies
  after a reboot.

## Capabilities (HTTP)

- `GET /capabilities`: `{"has_x_motor":bool,"has_endstops":bool,
  "has_probe":bool}`, from the pin map in use. A capability is its switch on
  and its function wired. `has_x_motor`: the X motor switch is on and an X
  motor is assigned. `has_endstops`: the homing switch is on and a home
  input is assigned. `has_probe`: the probe switch is on and a probe input
  is assigned.

## Backup (HTTP)

- `GET /backup`: one JSON file, `{"settings":{...}, "run":{units,
  jogStepDu, mode, "modes":[{mode, params}]},
  "threads":{...}, "pins":{...}}`. `settings` is the `settings` push's
  fields, `threads` the `/threads` shape, `params` the `<mode> status`
  line, `pins` the `GET /pins` shape.
- A `settings` section without a feature switch gets the default of a
  missing switch (see "Requests").
- `POST /backup`: refused unless idle. Every section present is validated
  first, then all are applied; absent sections and fields keep their current
  values. `pins` is saved only if it differs from the saved map, and applies
  after a reboot. Replies `ok` or the reason.
- In `settings`, the old fields `speedStart`, `speedRapid`,
  `speedManualMove` (steps/s) and `acceleration`, `deceleration`
  (steps/s²) with the axis suffix are converted to the `DuPerMin` and
  `DuPerSec2` fields, with the
  `motorSteps` and `screwPitchDu` of the same file, else of the active
  settings. A new field in the file wins. `speedRapid` wins over `speedManualMove`.

## G-code (HTTP)

- Dialects: `lathecode`, `linuxcnc`, `ngc`. An `ngc` program is a
  routine: its name, lower case without spaces, is its label; it must
  define `o<label> sub`; labels are unique; it is never active. A saved
  routine replaces the built-in one of the same label. Routines can not
  change while a G-code job runs.
- Upload body: `<dialect>\t<name>\n`, then the program text. Every upload
  is checked line by line, then dry run to count the steps (not for a
  routine). The count is unknown past a G38 move, a routine call, or a
  value known only at run time. A failure replies 400
  `{"ok":false,"error","line"}` (line 0 = not tied to a line). A LatheCode
  `; Axes expected:` header other than `Z+ points right, X+ points down` is
  refused.
- `POST /gcode/save`: stores a new program. `POST /gcode/run`: makes it the
  active program without saving. `POST /gcode/update?id=`: replaces a saved
  one; an empty dialect, name or program keeps the stored one.
  `POST /gcode/select?id=`: loads a saved program as the active one.
  Success replies `{"ok":true,"id","active"}`.
- `GET /gcode`: `{"programs":[{id, name, dialect, size}], "free", "active"}`.
  `active` is `null` or `{name, dialect, size, steps, id, version}` (`id`
  null if unsaved, `steps` = motion moves, null if unknown).
- `GET /gcode/program?id=` / `GET /gcode/active`: program text.
  `POST /gcode/delete?id=`.
- The web client does not send or upload a program that has an X word when
  `has_x_motor` is false, or a program or routine that has G38 when
  `has_probe` is false. A routine with an X word is sent. It shows the
  reason instead.
- `start` and `o<...> call` check the program text or call line, and dry
  run the job, before any move: an X word or an unguarded X move without
  the X motor capability gets `err X motor disabled` or `err X motor not wired`; a G38
  move without the probe capability gets `err probe disabled` or
  `err probe not wired`.
- Anything that would change the active program is refused while a G-code
  job runs.
- `POST /display-preview?ox=&oy=&s=`: baseline JPEG for the display's
  G-code panel. Image px = (`ox`, `oy`) + (Z, X) mm × `s`; used to place
  the live tool marker.

## Pushes (WebSocket)

Each pushed on connect, periodically where it's live telemetry, and again
right after any command changes that domain:

- `{"type":"status", ...}` -- axis work position (`posDu`)/moving, spindle RPM, `state`
  (`"idle"|"running"|"flashing"|"estop"` -- see `docs/design.md` "Machine
  state"). Web reads this field rather than inferring busy/flashing from
  anything else; firmware mirrors the same state to the display. Also the
  mode, `job` (`phase`, `pass`, `passes`), `feedOverride` (percent),
  `error` (why the last job ended
  on its own), `params` (the current mode's `name=value ...`) and
  `probeContact` (the live probe input state), `homing` (homing runs) and
  `wcs` (the active system, `"G54"`–`"G59"`). Each axis object has `homed`.
- `{"type":"capabilities","has_x_motor":bool,"has_endstops":bool,"has_probe":bool}`
  -- sent once on connect, same values as `GET /capabilities`.
- `{"type":"inputs","pins":{"<gpio>":0|1,...}}` -- raw level of each sampled
  connector pin, keyed by GPIO number as in `GET /pins`. Sampled pins: the
  non-encoder inputs of the active pin map and the unused connector pins.
  Outputs, encoders and reserved pins are not in it. Pushed only to clients
  that sent `inputs on`: once on subscribe, then on each change, with a
  minimum interval between pushes. The level has no invert applied. The
  firmware samples the pins only while a client is subscribed. `inputs off`
  or a disconnect ends the subscription. Both commands are handled by the
  WebSocket session and send no reply.
- `{"type":"settings", ...}` -- machine config.
- `{"type":"wifi", ...}` -- WiFi status.
- `{"type":"scan", ...}` -- WiFi scan results.
- `{"type":"metrics", ...}` -- system load/uptime/heap.
- `{"type":"probe","msg":"..."}` -- a G38 result; see "MDI".
- `{"type":"print","msg":"..."}` -- a `(PRINT, ...)` line; see "NGC".
- `{"type":"cal","msg":"..."}` -- a `cal` reply; see "Calibration".
- `{"type":"error","msg":"..."}` -- only on a failed command.
- `{"type":"reply","msg":"...","id":n}` -- see "Web" above.

## Introspection

- A `commands` query lists the live command set (syntax/description) --
  clients pull the grammar from the device rather than hardcoding it.

## Display UI upload (display UART)

Separate from the line-oriented command set above -- binary, and not a
protocol of our own design. The display stays on its stock, closed Nextion
firmware (see `docs/screen.md`); `main/lib/Nextion` just replays
Nextion's own documented TTL serial upload protocol (`whmi-wri`) to push a
`.tft` compiled from the `nxwright`/Nextion Editor source in `displays/nextion-5in/`:

- Try `connect` at each of `CONNECT_BAUD_CANDIDATES` (preceded by an empty
  command) until a `comok` reply.
- Send `whmi-wri <fileSize>,<UPLOAD_BAUD>,0`; switch to `UPLOAD_BAUD` if
  connected at another baud.
- Wait for ACK byte `0x05` -- display accepted the upload.
- Stream the `.tft` in `PACKET_SIZE` packets, waiting for `0x05` after each
  one (including the final, possibly partial, packet).
- Switch to `LINK_BAUD`, the UI's operating baud (`displays/nextion-5in/program.nx`),
  and wait for byte `0x88` (display flashed and restarted).

`main/lib/WebUi`'s `POST /update-display` drives this from the uploaded
`.tft`'s `Content-Length`, relaying the body straight through as it arrives.
