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
