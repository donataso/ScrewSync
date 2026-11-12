# Features

## Display

- One page per mode, with every parameter of that mode visible and editable.
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
- **Probe**: see [probe.md](probe.md).

Multi-pass modes show the phase and pass number, and can skip a pass.

## Threading

- Thread library: tabs per standard and a grid of designations. Picking one
  fills pitch, depth, taper and flank angle. Every value stays editable, and
  custom entries can be added.
- The thread table is editable on the web client.
- Alternating flank infeed: each pass shifts sideways so only one edge of
  the tool cuts, alternating sides.
- Spring pass at full depth.
- Multiple starts, left/right hand, external/internal, tapered threads.
- Reverse threading with an inverted tool, feeding away from the chuck.
- Thread mode, G-code G32 and Gear follow one helix, so a rerun lands in
  the same groove. See [manual.md](manual.md), "Thread alignment".

## Probing

- Touch off Z or X in either direction with a contact plate or a switch
  probe.
- Fast first touch, slow reprobe, then the axis is zeroed on the surface.
- A contact light shows the probe input live.

## G-code

- Paste or upload programs on the web client, with a toolpath preview.
  Made for [LatheCode](https://kachurovskiy.com/lathecode/).
- Programs are stored on the device and started from the display, which
  shows the preview, the tool position and step progress.
- Linear moves, spindle-synchronized threading (G32), and M0/M1 pauses.
- Programs with the wrong axis directions are refused; the web client
  offers to flip them.

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
- Travel clamp per axis.
- A lost display link stops any running job or jog.
- Firmware updates are refused while the machine is running.
- Live feed override while running, in every cutting mode but Thread.

## Web client

- Machine settings, with help on every field.
- Thread table editor.
- G-code programs.
- Wi-Fi, as an access point and as a station.
- Settings backup and restore.
- Device log viewer.
- Firmware and display UI updates from the browser.

## Planned

- DRO from glass scales. The hardware has the wiring; position is currently
  inferred from motor steps.
