# Homing

Home switches give each axis a known position. With them you get travel
limits, work systems G54–G59 and saved offsets.

## Cycle

- With a home input on each axis, the home icon in the header, the `home`
  command or the Home switch homes X, then Z. The machine must be idle and
  the motors on. `stop` ends it.
- Each axis moves toward the switch, backs off, finds it again slowly and
  pulls off. The axis position then reads its home position setting.
- Speeds, back-off, pull-off and home position are settings per axis.
- An axis is not homed after an estop, after its motor is turned off, after
  a reboot, and after a change of screw pitch, motor steps, direction
  invert, backlash, `maxTravelMm` or home position.
- A home signal while an axis moves trips the estop. The axes stop at
  once. When no axis moves, a signal does nothing.

## Require homing

- With home inputs and the setting on, start, jog, routines and G-code
  moves are refused with "not homed" until the axes are homed. Homing,
  estop, settings and motor on or off still work, and so do G-code lines
  that do not move.

## Inputs

- "Z home" and "X home" are one input each. They are not polled: each
  edge raises an interrupt.
- If one axis motor has a home input, each present axis motor must have
  one. Otherwise you cannot save the map.

## Tailstock sensor

- A tailstock sensor can share the X home input. While Z homes, X is off
  its switch, so a signal on this input is the tailstock. Triggering the
  wrong axis input during homing trips the estop.

## Sensors

- Use normally-closed (NC) sensors. A broken wire or a dead sensor then
  reads as triggered, so a switch cannot fail without a warning.
- The wiring below is for PNP sensors: OUT switches +V. NPN sensors need
  other wiring.
- The sensors run on 5 V. A sensor rated 6 V or more may or may not work
  at 5 V.

## Wiring

This is one suggestion: the
[817 optocoupler module](https://www.aliexpress.com/item/1005006377989607.html).
The terminals and jumpers below are for this module. The module removes the level problem: the ESP32 pin
never sees the sensor voltage. Each output channel is a switch between its
V and G terminals, closed while its input is active. The jumper of a
channel connects its G to the module's common GND.

Other wiring is possible but not covered here.

Connect the module outputs directly to ESP32 pins.

### Three sensors: X home, tailstock, Z home

The X home and tailstock channels are in series. Any triggered sensor opens
the chain.

| Jumper | State | Effect                                              |
|--------|-------|-----------------------------------------------------|
| ch1    | OFF   | ch1 G connects only to V2                           |
| ch2    | ON    | ch2 G to common GND                                 |
| ch3    | ON    | ch3 G to common GND                                 |

| From                    | To                  |
|-------------------------|---------------------|
| X home pin              | ch1 V1              |
| ch1 G (next to V1)      | ch2 V2              |
| Z home pin              | ch3 V3              |
| input G (any)           | GND                 |
| X home sensor, OUT      | IN1                 |
| tailstock sensor, OUT   | IN2                 |
| Z home sensor, OUT      | IN3                 |
| each sensor, +V         | 5V                  |
| each sensor, 0V         | GND                 |

### Two sensors: X home, Z home

No tailstock sensor. Simpler, but nothing detects the tailstock.

| Jumper | State | Effect                                              |
|--------|-------|-----------------------------------------------------|
| ch1    | ON    | ch1 G to common GND                                 |
| ch2    | ON    | ch2 G to common GND                                 |

| From                    | To                  |
|-------------------------|---------------------|
| X home pin              | ch1 V1              |
| Z home pin              | ch2 V2              |
| input G (any)           | GND                 |
| X home sensor, OUT      | IN1                 |
| Z home sensor, OUT      | IN2                 |
| each sensor, +V         | 5V                  |
| each sensor, 0V         | GND                 |

### Pin settings

In the pin map (see [pins.md](pins.md)), set both home inputs to:

- pull: up
- invert: off

All sensors idle: the channels are closed and the pin is low. Any sensor
triggered or a broken wire: the pin is high, which is active. Check it on
the Pins page: the pin's dot must light when you trigger each sensor.
