# Manual

How to operate ScrewSync. For connectors and flashing, see
[install.md](install.md).

## First setup

- Machine settings are on the web client only. See [Wi-Fi](#wi-fi).
- The settings show speeds in mm/min and accelerations in mm/s². With
  inch units, they show in/min and in/s².
- Axis directions: -Z moves toward the chuck, -X moves toward the
  centerline (ISO 841). If an axis moves the wrong way, toggle
  `invertDirectionZ` or `invertDirectionX`.
- Encoder: the spindle angle must increase when the spindle turns
  forward. If it decreases, toggle `invertEncoder`.
- Pins: to move a function to another connector pin, or to add switches
  and outputs, use the Pins page on the web client. See
  [pins.md](pins.md).

## Wi-Fi

1. With no saved network, the controller starts an open access point
   (AP). Its name is `ScrewSync-` followed by four characters of its
   hardware address.
2. Connect a phone or a computer to the AP. Open the IP address that the
   display shows (Settings, Network), then `/wifi`.
3. Type the SSID and the password of your network and push
   "Save & connect". The controller connects to that network as a
   station.
4. Read the new IP address on the display, or open
   `screwsync-xxxx.local` (the AP name in lower case).

- On the web client, the Network page shows the saved networks. Use it to
  scan, to join a network, and to remove one.
- To get the AP again, for example after you move the machine, set
  "Force AP" to On (Settings, Network).

## Screen

- Always visible: spindle RPM and angle, the mode name, and for each axis
  its toggle, position, →0 button and jog buttons, and the jog step.
- Tap the mode name to open the menu: the modes and Settings. The rest of
  the screen shows the page of the selected mode.
- Settings on the display: units (mm/inch), pitch entry (Pitch/TPI),
  Network (Wi-Fi On/Off, Force AP, status, IP), and the feature switches
  (see [Features](#features)).
- Header icons: estop, and with home switches, home and the work coordinate
  selector. See [Estop](#estop), [Homing](#homing) and
  [Work coordinates](#work-coordinates).
- Tap a field to edit it on the numpad. Each mode keeps its values.
- The display and the web client show the same state.

## Features

- The switches X motor, Homing and Probe are on the display (Settings) and
  on the web client (Device page, Features). A feature works when its switch
  is on and its function has pins (see [pins.md](pins.md)).
- A change is saved at once and applies after a reboot. Turning a feature
  off removes its functions from the saved pin map.
- Without the X motor, the X column stays on the display, but the X toggle
  stays off and the X jog buttons do nothing. The modes that need X
  (Turn, Thread, Part, Slot, Ellipse) are hidden on the display and
  the web client. The web client hides the X settings and checks.
- Without homing, `home` is refused. Without the probe, the Probe mode is
  hidden (it works without the X motor, on Z only) and the probe settings and checks are hidden on the web client.
- A G-code program that has an X word needs the X motor. A program or
  routine with G38 needs the probe. The web client does not send it, and the
  lathe refuses to run it. A routine can read `#<_has_x>` and `#<_has_probe>`
  to skip those parts. The built-in zero routine skips X without the X motor.

## Axes and jog

- Tap a jog button to move one step. Hold it to move continuously.
- The step buttons set the jog step. "..." types a custom step.
- The handwheels move the axes too. The travel for one handwheel turn is
  a setting for each axis.
- Physical jog buttons, a step cycle button and a step encoder can be
  added. See [pins.md](pins.md#jog).
- →0 opens a choice: "Go to zero" moves the axis to 0, "Set zero" makes
  the current position 0.
- Tap a position to type its value.
- Tap the spindle angle to zero it.
- Turn an axis toggle off to de-energize its motor and crank by hand. The
  readout is dimmed and does not count the cranking.
- At power-on, the Boot state setting sets the motors (see [Estop](#estop)).
  The motor on/off state is not saved.
- `power on|off [z|x]` turns the motors on or off, both without an axis.
- A jog on an axis that a running job drives stops the job, then moves the
  axis. This applies to every mode and to the handwheels.

## Estop

- An estop stops all motion and turns both motors off. It does not restart
  the controller.
- The Boot state setting in the web client sets the state at power-on:
  Estop, Estop off with motors off, or Estop off with motors on. After any
  other restart (watchdog, crash, brownout, `reboot`, firmware update), the
  machine starts in estop. An active estop input at start also keeps the
  estop.
- Estop icon in the header: tap it to trip the estop. In estop the display
  shows a big estop icon: tap the screen to clear it. Then turn the motors on
  again.
- In estop, start, jog, the handwheels, MDI, routines and motor on are
  refused. The serial and web commands are `estop on`, `estop off`,
  `power on` and `power off`.
- An estop button on an Estop input trips the estop at once. While the
  button is active, you cannot clear the estop.

## Homing

- With a home switch on each axis, the home icon in the header, the `home`
  command or the Home switch homes X, then Z. The machine must be idle and the motors on. `stop` ends it.
- Each axis moves toward the switch, backs off, finds it again slowly and
  pulls off. The axis position then reads its home position setting.
- An axis is not homed after an estop, after its motor is turned off, after
  a reboot, and after a change of screw pitch, motor steps, direction
  invert, backlash, `maxTravelMm` or home position.
- A home switch signal while an axis moves trips the estop. The axes stop at
  once. When no axis moves, a signal does nothing.

## Work coordinates

- The display and the web client show positions in the active work
  coordinate system. G54 is active at boot. `G10 L20 P0 Z0` sets zero in the
  active system, `P1`–`P6` in G54–G59. The Set zero switches and the probe
  set the active system.
- With home switches, select G54–G59 with the selector in the header, on the
  web MDI page, or with `G55` etc. Also before homing. The offsets are saved.
- Without home switches, only G54 exists. Its offset is lost at reboot.
- `G53 G0 Z… X…` moves in machine coordinates, for that line only.

## Require homing

- With home switches and the setting on, start, jog, routines and G-code
  moves are refused with "not homed" until the axes are homed. Homing,
  estop, settings and motor on or off still work, and so do G-code lines
  that do not move.

## Running a job

1. Touch the tool off on the work, or probe it, and set zero on Z and X
   there. This point is the engage point.
2. Select the mode and set its values.
3. Push START.

- Turn, Thread, Part, Slot and Ellipse start at zero. If the tool is not
  at zero, START asks "Go to zero first?".
- Multi-pass modes show the phase and the pass number. SKIP goes to the
  next pass.
- During a job, the feed override changes all feeds of the job, but not
  rapids. Thread has no override. Each start resets it to 100%.
- STOP ends the job with a controlled stop.

## Modes

- **Gear**: the axis follows the spindle at the set feed per revolution.
  Optional taper (Z only) and stop after a length.
- **Async**: the axis feeds per minute, independent of the spindle.
  Optional stop after a length.
- **Turn**: axis Z turns (depth on X), axis X faces (depth on Z). Internal
  reverses the depth direction.
- **Thread**: see [Threading](#threading).
- **Part**: X feeds in to the depth. Peck sets the depth of each pass;
  0 cuts the full depth in one pass. Internal reverses the X direction.
- **Slot**: Z feeds per minute with the spindle stopped. X steps deeper
  each pass. Internal reverses the X direction.
- **Ellipse**: a quarter ellipse on the end of the stock. The engage point
  is the corner. Length is along Z, depth is into X.
- **G-code**: see [G-code](#g-code).
- **Probe**: see [probe.md](probe.md).

## Spindle direction

- Turn, Thread, Part and Ellipse have a Fwd/Rev setting. Set it to agree
  with the spindle switch. The feed advances only in that rotation.
- If the spindle turns the other way, the job stops with "wrong spindle
  direction". This also occurs if the spindle rocks back when it brakes
  during a job.
- Use Rev for an inverted tool.
- Gear, Async and Slot ignore the spindle direction.

## Threading

- Push Library to open the thread library. Select a designation to fill
  pitch, depth, taper and flank angle. You can edit all values after this.
  Edit the thread table on the web client.
- Flank: each pass shifts sideways, so only one edge of the tool cuts.
- Spring: final pass at full depth.
- Thread in Rev keeps the selected hand, but feeds away from the chuck.
  Set the engage point at the chuck end, for example to thread out from a
  shoulder.

### Thread alignment

- Thread mode, G-code threading and Gear follow the spindle angle zero
  and the axis zero. Thus a rerun cuts in the same groove: stop, measure,
  run again.
- If you re-zero the angle or the axis, or reboot, the next cut starts a
  new helix.
- At start, Gear holds the axis for up to one spindle turn, until the
  spindle reaches the phase of the thread.

### Taper sign

- Taper is a half-angle. + means the diameter increases toward the chuck,
  for all feed directions, hands and spindle directions.
- Thread presets give + for external threads. Internal flips the sign.

## G-code

- Load programs on the web client. Start them on the display.
- Dialects: LatheCode, LinuxCNC and NGC routines. The web client finds
  LatheCode by its header and a routine by its `o<name> sub`, and takes
  any other text as LinuxCNC. You can change it.
- Program zero is the tool zero: X0 on the centerline (X is radius), Z0
  where the program expects it (LatheCode: the right edge of the stock).
- If the tool is not at zero, START asks you to confirm. The first move
  starts from the current tool position.
- Generate LatheCode programs with `AXES RIGHT DOWN`. The web client
  refuses other directions and offers to flip the axes.
- G1 feed is per minute along the path. G33 (LatheCode: G32) is
  synchronized to the spindle; K is the lead. The feed override applies to
  G1 only.
- `G10 L20 P0 Z… X…` makes the current position read the given values in the active work system (`P1`–`P6`: G54–G59).
- Motion pauses while the spindle is stopped and continues when it turns.
- M0/M1 pause until START. M2/M30 or STOP end the program.
- LinuxCNC programs can use NGC parameters, expressions, O-words and
  routine calls ([protocol.md](protocol.md), "NGC"). The web client shows
  no preview for them, and the step count is unknown when it depends on
  a probe result or a routine.

### Routines

- Save a program with the NGC dialect. Its name is its call:
  `probe` is `o<probe> call`. It must define `o<probe> sub` ...
  `o<probe> endsub`.
- The web client lists routines apart from programs. A routine can not be
  run as a program.

### MDI

- On the web client MDI tab or the serial console, type G-code lines
  (LinuxCNC), e.g. `G91 G0 Z-1`.
  The modes (G90/G91, G20/G21, F) stay between lines. STOP or `stop`
  stops the move and drops the queued lines.
- `G10 L20 P0 Z0` sets Z zero at the tool. `M110` sets the spindle angle
  to 0.

## Probe

- With a contact plate, the tool must be earthed (usually through the
  machine frame). Before you probe, touch the plate to the tool: the
  contact light must turn green.
- Set the probe distance and offset on the Probe panel or in the web
  settings. The cycle is a routine you can replace; see
  [probe.md](probe.md).

## Stops

- A job also stops if you disable one of its axes, or if the display link
  is lost.
- Travel limits apply only to a homed axis. The upper limit is the home
  position. The lower limit is `maxTravelMm` below it. The axis does not
  move past them. Without homing, an axis has no travel limit.
