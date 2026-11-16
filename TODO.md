# TODO

- DRO from glass scales, per axis: a present scale is the source of truth
  for position, otherwise motor steps. Needs a DRO panel; fixed stops in
  machine coordinates can be revisited then. Scale functions need their
  own default encoder filter, short enough for their fastest edges.
- add joystick support
- Pin map: test on the lathe (motors, spindle, probe, switches, output, a
  moved function).
- Pin map: reset to board defaults from the display menu, with a
  confirmation.
- Printable connector label with the user's connector names.
- Optional: live view of each input's level, to help identify wiring.
- G-code: G92.
- NOTICE.md: add Zephyr and its modules, and the web client's npm packages.
