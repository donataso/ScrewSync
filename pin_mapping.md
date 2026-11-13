# Pin mapping

Proposal: assign pins to functions from the web UI, not in `main/app.overlay`.

## Board / display / target

- **Board:** the MCU and its connectors. It defines:
  - the connector map (connector → pin → label), which the web UI shows,
  - the pins that are free to assign,
  - the PCNT units that are free,
  - the default pin map.
- **Display:** its own code. A Nextion layout, or an LVGL UI with its driver. Each layout or size is a separate display. Nextion layouts use the same Nextion driver and wiring. The firmware does not detect the display.
- **Target:** one board + one display + how the display is connected (bus and pins). Only combinations that someone owns.

## Board data

- A custom devicetree node (for example `screwsync,connector`) in the board overlay, with a YAML binding in `dts/bindings/`.
- The Zephyr `DT_FOREACH_CHILD` / `DT_PROP` macros make a `constexpr` C++ table from it at compile time. No pre-build script.
- The firmware serves the table to the web UI through the API.
- Host unit tests use a fake table.

## Decisions

- The map applies after a reboot. Live changes are not necessary.
- Two kinds of roles: fixed roles and generic slots.
- The user selects only from the pins that the board marks as free.

## Storage

- A `PinMap` blob in Settings/PersistBlob. One entry per role: `{role, gpio, activeLow, pull}`.
- If no blob is saved, the firmware uses the board's default map.
- Recovery: a serial command, or a button held at boot, sets the map back to defaults.

## Roles

- Fixed: Z/X step/dir/ena, spindle A/B, Z/X handwheel A/B, probe.
- Generic inputs: each slot is linked to an action (jog, e-stop, start, ...).
- Generic outputs: each slot is linked to a state (running, alarm, ...).

## Firmware

- Plain GPIO: make the `gpio_dt_spec` at runtime from the pin number (`gpio0` or `gpio1`, as the pin number sets).
- Encoders: send the pins to PCNT through the GPIO matrix at init (`esp_rom_gpio_connect_in_signal()` or `pinctrl_configure_pins()`). The PCNT unit and channel setup stays in devicetree.
- The firmware rejects conflicts and pins that the board does not mark as free. The web UI only shows the result.

## Encoders

- The ESP32-S3 has 4 PCNT units. One unit decodes one quadrature encoder in hardware, with no CPU load.
- The two channels of a unit add into one counter, so one unit cannot count two encoders.
- Each encoder slot has a type:
  - `hw`: a PCNT unit, assigned automatically. The spindle is always `hw`.
  - `sw`: decoded from GPIO interrupts on both edges of A and B. Only for slow encoders (handwheels, MPG knobs).
- The firmware rejects a map that needs more `hw` encoders than the board has free PCNT units.
- The number of `sw` encoders is not fixed. It depends on the total edge rate, because too many interrupts can delay the stepper step timing.
- Optional: set a maximum rate for `sw` encoders, so the UI can warn you.

## Web UI

- The board's connector map.
- A table: role → pin dropdown. The dropdown shows only pins that are free and can do that role.
- It shows conflicts in red, and has invert/pull toggles and a "Save & reboot" button.
- Optional: a live view of each input's level, to help identify wiring.
