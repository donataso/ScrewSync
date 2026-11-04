# Manual notes

Easy-to-miss behaviour. Not a full manual.

## Hardware

- Connectors are not where the case labels say. See
  [install.md](install.md).

## Probe

- With a contact plate, the tool must be earthed (usually through the
  machine frame). Before probing, touch the plate to the tool: the contact
  light must turn green.

## Jog overrides a running job

- TODO: mention that any jog (handwheel, display button, serial) on an axis
  the running mode drives stops the job and moves the axis. Applies to gear
  mode too.

## Axis directions

- -Z toward the chuck, -X toward the centerline, as on standard (ISO 841)
  CNC lathes. If an axis moves the wrong way, toggle its `invertDirection`
  setting.

## G-code

- Programs are loaded on the web and started on the display.
- Program zero is the tool's zero: X0 on the centerline (X is radius),
  Z0 where the program expects it (LatheCode: right edge of the stock).
  Starting off zero asks to confirm; the first move goes from where the
  tool is.
- LatheCode programs must be generated with `AXES RIGHT DOWN`; others are
  refused, and the web offers to flip the wrong axes.
- G1 feed is per minute along the path; G32 is spindle-synchronized. Feed
  override applies to G1/G32, not G0.
- Motion pauses while the spindle is stopped and continues when it turns.
- M0/M1 pause until START; STOP ends the program. M2/M30 end it.
- A jog on X or Z ends the program.

## Thread alignment

- Threads (Thread mode, G-code G32, Gear) follow the spindle angle zero and
  the axis zero, so a rerun lands in the same groove. Re-zeroing the angle
  or the axis, or a reboot, starts a new helix.
- Gear holds the axis for up to one spindle turn after start, until the
  spindle reaches the thread's phase.

## Encoder direction

- The spindle angle must increase when the spindle turns forward, as on
  standard CNC lathes. If it decreases, toggle the `invertEncoder` setting.

## Spindle direction (Fwd/Rev)

- Turn, Thread, Part and Ellipse have a spindle Fwd/Rev setting. Set it to
  match the spindle switch: the feed only advances in that rotation.
- Turning the other way stops the job with "wrong spindle direction". It
  trips as soon as the spindle moves backward past the encoder backlash
  setting, including a spindle that rocks back when braking mid-job.
- Rev is for an inverted tool. Thread in Rev keeps the selected hand but
  feeds away from the chuck, so set the engage point at the chuck end
  (e.g. threading out from a shoulder).
- Gear, Async and Slot ignore spindle direction.

## Taper sign

- Taper is a half-angle; + means the diameter grows toward the chuck,
  regardless of feed direction, hand or spindle direction.
- Thread presets give + for external threads; switching to internal
  flips the sign.
