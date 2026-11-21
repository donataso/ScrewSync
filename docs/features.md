# Features

## Display

- One page per mode, with every parameter of that mode visible and editable.
- Header icons: estop, and with home switches, home and the work coordinate
  selector.
- Always visible: Z/X position, axis enable toggles, spindle RPM and angle,
  current mode, start/stop, mm/inch, jog buttons and jog step.
- Numeric entry on an on-screen keypad with a computer numpad layout.
- Each mode's parameters are remembered.
- The display and the web client show the same state; a change on one shows
  up on the other.

## Modes

- **Gear**: feed per spindle rev (presets or TPI entry), direction, Z or X,
  optional taper, optional stop after a length.
- **Async**: feed per minute, independent of spindle rotation.
- **Turn**: turning (Z) or facing (X), multi-pass, external/internal.
- **Thread**: see below.
- **Part**: part-off with an optional peck depth.
- **Slot**: slot cutting with the spindle stopped.
- **Ellipse**: quarter ellipse on the stock end, convex or concave.
- **G-code**: see below.
- **Probe**: see [probe.md](probe.md). Needs only the probe, not the X motor.

Multi-pass modes show the phase and pass number, and can skip a pass.

## Threading

- Thread library: tabs per standard and a grid of designations. Picking one
  fills pitch, depth, taper and flank angle. Every value stays editable, and
  custom entries can be added.
- The thread table is editable on the web client.
- Constant chip area per pass (Sandvik infeed), with a lighter first pass.
- Flank infeed: each pass shifts sideways so only one edge of the tool
  cuts. One side (leading edge) or alternating sides.
- Spring pass at full depth.
- Multiple starts, left/right hand, external/internal, tapered threads.
- Reverse threading with an inverted tool, feeding away from the chuck.
- Thread mode, G-code threading and Gear follow one helix, so a rerun lands in
  the same groove. See [manual.md](manual.md), "Thread alignment".

## Probing

- Touch off Z or X in either direction with a contact plate or a switch
  probe.
- Fast first touch, slow reprobe, then the axis is zeroed on the surface.
- The cycle is an NGC routine; a saved routine can replace it.
- A contact light shows the probe input live.

## G-code

- Paste or upload programs on the web client, with a toolpath preview.
  [LatheCode](https://kachurovskiy.com/lathecode/) and standard LinuxCNC
  G-code.
- Programs are stored on the device and started from the display, which
  shows the preview, the tool position and step progress.
- Linear moves, spindle-synchronized threading (G33; LatheCode G32),
  G10 L20 position setting, work coordinate systems G54–G59 (with home
  switches), G53, and M0/M1 pauses.
- MDI: G-code lines on the web client or the serial console, with modes
  kept between lines. G38 probe moves. Multiline paste, with a status per
  line.
- LinuxCNC NGC: parameters, expressions, O-word subs, conditions and
  loops, G38 results, settings as parameters. Run at execution time.
- NGC routines stored on the device and called by name, also as a
  command.
- Programs with the wrong axis directions are refused; the web client
  offers to flip them.

## Homing

- One home switch per axis. `home` homes X, then Z: fast pass, back off,
  slow pass, pull off. Start it from the display, the Home switch or the
  protocol.
- Home inputs use interrupts. A tailstock switch can share the X input.
- Speeds, back-off, pull-off and home position are settings per axis.
- Optional "require homing": no moves until the axes are homed.
- A homed axis has travel limits from its home position.
- With home switches, work systems G54–G59 and saved offsets.
- See [homing.md](homing.md).

## Manual control

- Jog: tap for one step, hold to move continuously with acceleration.
- Handwheels on both axes.
- Separate acceleration and deceleration per axis. Friction helps the motor
  stop, so an axis can stop faster than it starts.
- Per-axis enable/disable, de-energizing the motor for manual cranking.
- Zero either axis, or go to zero with a confirmation.

## Safety

- Spindle direction check: Turn, Thread, Part and Ellipse stop if the
  spindle turns the wrong way.
- Any jog on an axis a running job drives stops the job.
- Starting a job off zero asks for confirmation.
- Boot state setting: start in estop, with estop off, or with estop off and
  motors on. Estop stops all motion and turns the motors off, with no
  restart. Estop icon on the display, estop inputs and an estop
  output.
- A home switch signal while an axis moves trips the estop.
- A lost display link stops any running job or jog.
- Firmware updates are refused while the machine is running.
- Live feed override while running, in every cutting mode but Thread.

## Feature switches

- X motor, homing and probe can each be turned off, on the display or the
  web client. A feature needs its switch on and its pins assigned.
- A missing feature is hidden in the pin editor, the modes that need it and
  the web client, and its commands are refused. The display keeps the X
  column, with X off.
- G-code with an X word (programs) or G38 is refused when that feature is
  missing. Routines can skip parts with `#<_has_x>` and `#<_has_probe>`.

## Web client

- Machine settings, with help on every field.
- Thread table editor.
- G-code programs and routines.
- MDI console.
- Pin map editor, and a printable pin map sheet.
- Wi-Fi, as an access point and as a station.
- Settings backup and restore.
- Device log viewer.
- Firmware and display UI updates from the browser.

## Planned

- DRO from glass scales. The hardware has the wiring; position is currently
  inferred from motor steps.
