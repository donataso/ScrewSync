# Calibration

A web client tab with checks for one-time machine setup. Each check shows the measured value next to the current setting, and an Apply button writes it with `settings set`. There is no display UI.

All checks need the machine idle. A check can be stopped at any time.

## Checks

| Check             | Operator does                                          | Firmware does                            | Setting                              |
|-------------------|--------------------------------------------------------|------------------------------------------|--------------------------------------|
| Spindle direction | Turns the chuck forward by hand                        | Reports the raw encoder count            | `invertEncoder`                      |
| Encoder PPR       | Turns the chuck exactly N turns by hand, enters N      | Reports the raw encoder count            | `encoderPpr`                         |
| Axis direction    | Jogs Z+/X+, selects which way the carriage moved       | Normal jog                               | `invertDirectionZ`/`X`               |
| Steps/mm          | Enters the dial indicator reading after a set move     | Moves a set distance                     | `screwPitchDuZ`/`X`                  |
| Backlash          | Sets the probe against the workpiece                   | `o<backlash>` routine (below)            | `backlashDuZ`/`X`                    |
| Probe polarity    | Keeps the probe open                                   | Reports the probe input                  | `probePolarityNc`                    |

Without the probe capability, the probe polarity and backlash checks are
hidden. Without the X motor capability, the X rows are hidden. See
`docs/manual.md`, "Features".

Order in the tab: probe polarity, spindle direction, encoder PPR, axis direction, steps/mm, backlash. Each later check depends on the earlier ones.

### Spindle direction and PPR

The web client takes the count delta between Start and Done. Direction: a negative delta gives `invertEncoder` toggled. PPR: `delta / (2 × N)`, because the count is 2x-decoded.

### Steps/mm

The new `screwPitchDu` is `screwPitchDu × measured / commanded`.

### Backlash (release measurement)

Each axis has one setting. The check measures from both sides and applies the average. If the two sides are far apart, it shows a warning: the probe or the machine rigidity has a problem.

The web client calls the built-in routine `o<backlash>` (see `docs/protocol.md`, "NGC") and reads its printed result. Per side:

1. `G38.2` toward the workpiece. The motor position there is P1.
2. `G38.4` in reverse at reprobe feed until the contact opens. The motor position there is P2.
3. `G1` back off.
4. Repeat. The result is the average of |P2 − P1| over the runs.

While the motor takes up the backlash, the carriage does not move, so the contact stays closed. The result includes the probe's release hysteresis and the elastic flex of the machine. Use a contact plate, not a switch. A switch adds its hysteresis.

P1 and P2 are motor positions, so the backlash setting does not change the result.

The first touch uses the probe feed and the probe distance setting. The later moves use the reprobe feed and the back-off setting.

## Protocol

- `cal count`: the raw spindle encoder count.

Over the WebSocket, `cal` replies are typed pushes, `{"type":"cal","msg":"<reply>"}`.
