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
  `{"type":"cmd","line":"<verb> [args...]"}`. No reply envelope, no `id` --
  only one command in flight per client, so a reply is just whichever typed
  push below reflects the change; a failure instead gets
  `{"type":"error","msg":"<reason>"}`.
- All JSON on the wire -- either direction -- is built with
  `zephyr/data/json.h` (`json_obj_encode_buf`/descriptors), never by hand
  with `snprintf`/string concatenation.

## Requests

- `<verb> [args...]` (e.g. `move Z 12.500`, `zero X`, `mode thread`).
- `settings set-all <json>`: applies every field present in one JSON object
  (bulk import, or a settings form saving several fields at once), through
  the same per-field validation as `settings set <field> <value>`, instead
  of one round trip per field.
- A command for a feature not compiled into this build (e.g. X axis, DRO,
  a mode requiring the X motor) responds with a distinct "not compiled"
  error, not a generic unknown-command error -- lets a client tell "this
  firmware build doesn't have this" apart from "typo".
- Serial/UART replies stay `ok [data]` / `err <reason>` (unchanged).
- Any command on any transport gets `err busy: flashing`/`err busy: estop`
  instead of running while machine state is `flashing` or `estop`.
- `jog step [<deci-microns>|next]`: shows or sets the jog step. `next`
  selects the next preset, then the custom step, then the first preset.
- `estop` / `estop ack`: `estop` alone replies `ok state=<name>`. `estop ack`
  is the one command let through while tripped -- it clears the EStop and
  reboots.

## Modes

- `mode [<name>]` selects (refused while running) or reports mode, run
  state, phase, pass and the last job error. `start [zero]` (`zero`: rapid Z,
  then X to 0 first), `stop`, `skip` (next
  depth pass of a multi-pass mode).
- `override [status] | + | - | reset`: feed override of the running job,
  percent of the set feed, clamped to `Mode::OVERRIDE_*_PCT`. Refused when
  idle or in thread; back to 100 on each `start`.
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
- `probe go z-|z+|x-|x+` starts a touch-off cycle on that axis/direction
  (selects probe mode and starts it in one call); a plain `start` is refused. `probe [status]` adds
  `contact=0|1`, the live probe input state, to the usual `name=value` line.

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
- `GET /pins/functions`: the function table.
- `POST /pins`: validates and saves a map. Refused unless idle. Applies
  after a reboot.

## Backup (HTTP)

- `GET /backup`: one JSON file, `{"settings":{...}, "run":{units,
  jogStepDu, mode, disabledZ, disabledX, "modes":[{mode, params}]},
  "threads":{...}, "pins":{...}}`. `settings` is the `settings` push's
  fields, `threads` the `/threads` shape, `params` the `<mode> status`
  line, `pins` the `GET /pins` shape.
- `POST /backup`: refused unless idle. Every section present is validated
  first, then all are applied; absent sections and fields keep their current
  values. `pins` is saved only if it differs from the saved map, and applies
  after a reboot. Replies `ok` or the reason.

## G-code (HTTP)

- Upload body: `<flavor>\t<name>\n`, then the program text. Every upload
  is validated by a dry run through the flavor; a failure replies 400
  `{"ok":false,"error","line"}` (line 0 = not tied to a line). A LatheCode
  `; Axes expected:` header other than `Z+ points right, X+ points down` is
  refused.
- `POST /gcode/save`: stores a new program. `POST /gcode/run`: makes it the
  active program without saving. `POST /gcode/update?id=`: replaces a saved
  one; an empty flavor, name or program keeps the stored one.
  `POST /gcode/select?id=`: loads a saved program as the active one.
  Success replies `{"ok":true,"id","active"}`.
- `GET /gcode`: `{"programs":[{id, name, flavor, size}], "free", "active"}`.
  `active` is `null` or `{name, flavor, size, steps, id, version}` (`id`
  null if unsaved, `steps` = motion moves).
- `GET /gcode/program?id=` / `GET /gcode/active`: program text.
  `POST /gcode/delete?id=`.
- Anything that would change the active program is refused while a G-code
  job runs.
- `POST /display-preview?ox=&oy=&s=`: baseline JPEG for the display's
  G-code panel. Image px = (`ox`, `oy`) + (Z, X) mm × `s`; used to place
  the live tool marker.

## Pushes (WebSocket)

Each pushed on connect, periodically where it's live telemetry, and again
right after any command changes that domain:

- `{"type":"status", ...}` -- axis position/moving, spindle RPM, `state`
  (`"idle"|"running"|"flashing"|"estop"` -- see `docs/design.md` "Machine
  state"). Web reads this field rather than inferring busy/flashing from
  anything else; firmware mirrors the same state to the display. Also the
  mode, `job` (`phase`, `pass`, `passes`), `feedOverride` (percent),
  `error` (why the last job ended
  on its own), `params` (the current mode's `name=value ...`) and
  `probeContact` (the live probe input state).
- `{"type":"settings", ...}` -- machine config.
- `{"type":"wifi", ...}` -- WiFi status.
- `{"type":"scan", ...}` -- WiFi scan results.
- `{"type":"metrics", ...}` -- system load/uptime/heap.
- `{"type":"error","msg":"..."}` -- only on a failed command.

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
