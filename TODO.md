# TODO

- G-code and ngc export and import.
- Custom modes as ngc routines. Native modes stay. The mode and its fields
  are structured comments in the file, so export and import carry them:
  ```
  (MODE, Knurl)
  (PARAM, depth, mm, 0.3, required)
  (PARAM, passes, count, 3)
  (PARAM, dir, choice, -|+, -)
  o<knurl> sub
  ...
  o<knurl> endsub
  ```
  One generic display page with parameter slots and Start/Stop. Values
  persist in `RunSettings` by routine name. The header is parsed at save
  on the device and on the web client. Open: comment syntax, field types,
  named or positional binding, values of removed fields, pass number and
  skip pass.
- Full ngc syntax: if/elseif/else, loops (while, do, repeat), subroutines.
- DRO from glass scales, per axis: a present scale is the source of truth
  for position, otherwise motor steps. Needs a DRO panel; fixed stops in
  machine coordinates can be revisited then. Scale functions need their
  own default encoder filter, short enough for their fastest edges.
- add joystick support
- Pin map: reset to board defaults from the display menu, with a
  confirmation.
- G-code: G92.
- Default pass count per entry in the thread library.
- Stepper driver alarm input support.
- Thread start transient. At the helix phase, the Z sync target starts at
  full speed, but Z accelerates from rest: it lags, then overshoots to catch
  up. X plunges at the same moment, so the tool is off the helix for about
  v²/a of Z travel (1.75 mm pitch, 350 rpm, 25 mm/s² → about 10 mm). This
  ruins an existing thread when a thread is extended. Higher Z acceleration
  makes it shorter. Fix: a planned Z ramp from the start point that joins
  the helix tangentially (v²/2a), X held out until Z is on the helix. No
  travel behind the start point (tailstock, shoulders). Lead-in = ramp plus
  X plunge travel. Open: show it read-only (needs RPM), or a set lead-in
  that the firmware checks before each pass.
