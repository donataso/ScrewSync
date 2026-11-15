# Probe mode

Touch off an axis: move until the probe makes contact, then zero the axis.

## Hardware

The probe is a digital input with a pull-up and an RC filter against stepper noise. Its pin is set in the board overlay (`boards/<board>/board.overlay`), and its polarity (NO/NC) is in settings. Ideas for a simple DIY probe:

- **Contact plate:** a plastic block with a conductor on one face, wired to the probe pin and held against the workpiece. The tool must be earthed, which it usually is through the machine frame. DC GND should be connected to AC Earth. Contact pulls the pin to ground.
- **Switch:** a small switch, for example a low-profile keyboard switch, in a holder placed against the workpiece. Both wires go to the switch, so no earthing is needed. After it triggers, the switch has only a little travel left, so the probe feed must be slow enough for the axis to stop within that travel.

For both types, `offset` is the distance from the trigger point to the workpiece surface. For a plate that is its thickness; for a switch it is measured once.

## Settings

- probe polarity: NO or NC
- probe feed: speed per minute for the first touch
- reprobe feed: speed per minute for the back-offs and the reprobe
- back off: retract after each touch
- distance: the furthest the first touch may travel (0 = not set)
- offset: trigger point to surface

## Cycle

The cycle is the built-in NGC routine `o<probe>`
(`main/lib/GcodeStore/routines/probe.ngc`). A saved `ngc` routine named
`probe` replaces it. The display arrows send `o<probe> call [<axis>]
[<dir>]`; see `docs/protocol.md`, "NGC".

1. Refuse to start if the distance is not set, or if the probe input
   already shows contact.
2. `G38.2` toward the workpiece at probe feed, at most the distance. On
   contact, an interrupt stops the axis immediately, with no decel.
3. `G1` back off at reprobe feed.
4. `G38.2` at reprobe feed, at most the back-off plus a small margin.
5. `G1` back off again.
6. `G10 L20` so that the surface reads 0. The surface is at
   `contact + dir × offset`, where `dir` is the probe direction (±1).

Distances are carriage positions, so the axis backlash setting is applied on each reversal.

X shows plain position, so `offset` is applied once. To zero X on the centerline, enter the radius on the X readout afterwards.

Errors (shown as the job error) end the cycle with the axis stopped:

- contact present at start
- no contact within the distance (first touch) or within the reprobe limit
- contact still present after a back-off
- a contact interrupt with the contact already gone (noise must be solved at the probe)

A jog on the axis stops the cycle, as it does for any job.

## Display

- In the mode picker, the Probe entry uses the Settings colour, so it stands apart from the cutting modes. It is a panel, not a mode.
- Triggering the probe by hand before probing confirms the wiring and, for a plate, the tool's earth.

| Row        | Col 1                                        | Col 2         | Col 3                  |
|------------|----------------------------------------------|---------------|------------------------|
| 1          | Distance + value (2-col)                     |               | Offset + value (2-col) |
| 2          |                                              | `X ↑`         |                        |
| 3          | `← Z`                                        | contact light | `Z →`                  |
| 4          |                                              | `↓ X`         |                        |
| 5          | Phase or last error, plain text (full width) |               |                        |
| start slot | STOP while running                           |               |                        |

- The arrows match the jog panel: up is X-, toward the centerline.
- The contact light is grey when open and green when triggered.
- Direction buttons use the jog button colours, and are greyed out and inactive while their axis is disabled.
- The start slot is hidden while idle; the direction buttons start the cycle.
- There is no feed override row.
