# TODO

- Distinct "not compiled" WS/serial error for a command touching a feature
  not built into this firmware image (X axis, DRO, a mode requiring the X
  motor), instead of falling through to a generic unknown-command error.
  Needs compile-time axis/feature gating first -- none exists yet (Stepper/
  Sync/Jog/Command all build Z and X unconditionally).
- estop: physical trigger (button on some pin) and on-screen ack UI -- the
  Machine/Command/Stepper-side trip+block+ack logic is done, this is just
  the missing input/UI
- DRO from glass scales, per axis: a present scale is the source of truth
  for position, otherwise motor steps. Needs a DRO panel; fixed stops in
  machine coordinates can be revisited then.
- Hide an absent X motor or DRO everywhere in the UI (display and web),
  with no greyed-out placeholders, including the modes that need X.
- add joystick support

