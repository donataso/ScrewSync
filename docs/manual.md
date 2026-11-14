# Manual

How to operate ScrewSync. For connectors and flashing, see
[install.md](install.md).

## First setup

- Machine settings are on the web client only. See [Wi-Fi](#wi-fi).
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
- Settings on the display: units (mm/inch), pitch entry (Pitch/TPI), and
  Network (Wi-Fi On/Off, Force AP, status, IP).
- Tap a field to edit it on the numpad. Each mode keeps its values.
- The display and the web client show the same state.

## Axes and jog

- Tap a jog button to move one step. Hold it to move continuously.
- The step buttons set the jog step. "..." types a custom step.
- The handwheels move the axes too.
- Physical jog buttons, a step cycle button and a step encoder can be
  added. See [pins.md](pins.md#jog).
- →0 opens a choice: "Go to zero" moves the axis to 0, "Set zero" makes
  the current position 0.
- Tap a position to type its value.
- Tap the spindle angle to zero it.
- Turn an axis toggle off to de-energize its motor and crank by hand. The
  readout is dimmed and does not count the cranking.
- A jog on an axis that a running job drives stops the job, then moves the
  axis. This applies to every mode and to the handwheels.

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
  rapids. Thread and Probe have no override. Each start resets it to 100%.
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

- Thread mode, G32 and Gear follow the spindle angle zero and the axis
  zero. Thus a rerun cuts in the same groove: stop, measure, run again.
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
- Program zero is the tool zero: X0 on the centerline (X is radius), Z0
  where the program expects it (LatheCode: the right edge of the stock).
- If the tool is not at zero, START asks you to confirm. The first move
  starts from the current tool position.
- Generate LatheCode programs with `AXES RIGHT DOWN`. The web client
  refuses other directions and offers to flip the axes.
- G1 feed is per minute along the path. G32 is synchronized to the
  spindle. The feed override applies to G1 and G32, not to G0.
- Motion pauses while the spindle is stopped and continues when it turns.
- M0/M1 pause until START. M2/M30 or STOP end the program.

## Probe

- With a contact plate, the tool must be earthed (usually through the
  machine frame). Before you probe, touch the plate to the tool: the
  contact light must turn green.

## Stops

- A job also stops if you disable one of its axes, or if the display link
  is lost.
- Each axis has a travel clamp around zero (`maxTravelMm`). The axis does
  not move past it.
