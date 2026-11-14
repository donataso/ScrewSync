# Pin mapping

Assign each function (motors, encoders, switches, outputs) to any free
connector pin from the web client, Pins page. No rebuild.

## Use

1. Select a function, then its start pin. Or select a free pin, then a
   function that can start there.
2. Set the options of each signal: invert, and for inputs pull (none, up,
   down).
3. Push "Save and reboot". The new map applies after the reboot.

- A function uses pins next to each other in one connector, in its signal
  order (for example ENA, DIR, STEP).
- Reserved pins are locked. Strapping pins give a warning: they can stop
  the board from booting if the wiring pulls them at reset.
- You can only save when the machine is idle and the map has no errors.
- Click a connector name to rename it for your wiring. An empty name sets
  the board's name back.
- Some functions can have more than one assignment, for example two Stop
  switches. Contact on any Probe input is contact.

## Encoders: hw and sw

- **hw**: a hardware pulse counter (PCNT). No CPU load, fast. The board has
  few PCNT units. The spindle encoder is always hw.
- **sw**: interrupts on each edge, decoded in software. Uses CPU time. Only
  for slow encoders, such as handwheels. Use it when no PCNT unit is free.
- Both count all four edges of A and B. Contact bounce on one line counts
  up and down again, so it cancels.

## Glitch filter

Each encoder has a filter, in µs:

- **hw**: the PCNT glitch filter. The counter ignores pulses shorter than
  this. The hardware sets the maximum.
- **sw**: the debounce time. A and B are read only after both are steady
  for this time. It is rounded up to the kernel tick.
- Handwheels default to the hw maximum, for mechanical contacts.
- A filter that is too long drops real edges. At save, the web client and
  the firmware check that the filter passes the edges of the function's
  top speed at the PPR from the settings. A filter of 0 on hw is not
  checked.
- A later PPR change in the settings is not checked. At boot, an
  out-of-limit encoder gives only a log warning.

## Switches

- Inputs are polled and debounced. A switch acts once, on the press.
- A switch held at boot does not act.
- A switch sends the same command as the display, with the same refusal
  rules. A refused command shows as a toast on the display.
- "Start here" starts without the "Go to zero first?" question.
- "Go to zero" moves Z to 0, then X to 0.

## Feed override encoder

- Each pulse changes the feed override of the running job by 1%.
- Turns while no job with a feed override runs are ignored, with no toast.

## Jog

- Jog Z+, Z-, X+, X- buttons work like the display's jog buttons: a short
  press moves one step, a long press moves until the release.
- If a jog is refused (for example, the axis is off), the button does
  nothing more until you release it.
- "Jog step cycle" selects the next preset, then the custom step, then the
  first preset again.
- "Jog step encoder" sets a custom step. Each detent changes the step by
  the last digit that the display shows. A faster turn makes larger changes. On a preset,
  the first detent starts from that preset. The step is saved when the
  encoder stops.
- The step encoder counts one per quadrature cycle. To reverse it, invert
  A or B.

## Outputs

- "Running" is on while a job runs.

## Faults and recovery

- If no map is saved, the board defaults apply.
- At boot, an invalid assignment (bad pin, clash) is skipped and logged.
  In a clash, the first assignment stays.
- If a required function (Z motor, spindle encoder) has no pins, the
  machine has a pins fault. Axis moves are refused with `err pins: ...`
  and the display shows a toast. Settings and the web client still work.
- A motor with no pins refuses moves.
- To get the board defaults back, send `pins reset` on the serial
  console, then reboot. `pins` shows which map is in use.
- The pin map is part of the device backup. A restore applies it after a
  reboot.

## nanoels H5

The handwheel connectors have B before A. Thus the default map has A and B
swapped compared with the case labels. If a handwheel turns the wrong way,
toggle `invertHandwheelZ` or `invertHandwheelX`.
