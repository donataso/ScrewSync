# Pin mapping

Functions are assigned to connector pins from the web UI. The map applies after a reboot.

## Board / display / target

- **Board:** the MCU and its connectors. It defines the connector map, the reserved and strapping pins, the PCNT units and the default pin map.
- **Display:** its own code. A Nextion layout, or an LVGL UI with its driver. Each layout or size is a separate display. The firmware does not detect the display.
- **Target:** one board + one display + how the display is connected. Only combinations that someone owns.

## Board data

- A `screwsync,connectors` node in the board overlay. Binding: `main/dts/bindings/screwsync,connectors.yaml`.
- One child per connector: title, row, column and pins. Power pins use `PIN_5V` / `PIN_GND` from `main/include/dt-bindings/screwsync/pins.h`.
- The node also holds the strapping pins, the reserved pins with a note each, and the default pins of each function in signal order.
- The default pins of a function must be next to each other in one connector.
- `main/lib/Pins` builds the board table from the node with `DT_FOREACH` macros. No pre-build script.
- The PCNT unit count is the number of PCNT units in the devicetree.

## Functions

The function table is only in the firmware. `GET /pins/functions` sends it to the web UI. To add a function, add a row to the table and the code that uses its pins.

| Function        | Signals        | Dir | Required | Exclusive | Encoder  | Action               |
|-----------------|----------------|-----|----------|-----------|----------|----------------------|
| Z motor         | ENA, DIR, STEP | out | yes      | yes       | -        | -                    |
| X motor         | ENA, DIR, STEP | out | no       | yes       | -        | -                    |
| Spindle encoder | A, B           | in  | yes      | yes       | hw       | -                    |
| Z handwheel     | A, B           | in  | no       | yes       | hw or sw | -                    |
| X handwheel     | A, B           | in  | no       | yes       | hw or sw | -                    |
| Probe           | SIG            | in  | no       | no        | -        | -                    |
| Start here      | SIG            | in  | no       | no        | -        | `start`              |
| Stop            | SIG            | in  | no       | no        | -        | `stop`               |
| Set zero        | SIG            | in  | no       | no        | -        | zero Z and X         |
| Set zero Z      | SIG            | in  | no       | no        | -        | `zero z`             |
| Set zero X      | SIG            | in  | no       | no        | -        | `zero x`             |
| Go to zero      | SIG            | in  | no       | no        | -        | `move zero`          |
| Go to zero Z    | SIG            | in  | no       | no        | -        | `move z to 0`        |
| Go to zero X    | SIG            | in  | no       | no        | -        | `move x to 0`        |
| Mode `<name>`   | SIG            | in  | no       | no        | -        | `mode <name>`        |
| Running         | SIG            | out | no       | no        | -        | on while a job runs  |

- Mode switches: one function for each mode in `Mode::NAMES`.
- A function fills pins next to each other in one connector, in its signal order.
- Each signal has invert and, for inputs, pull (none, up, down).
- An exclusive function has at most one assignment. A non-exclusive function can have more (for example, two Stop switches).
- A motor with no pins refuses moves. A handwheel with no pins does not count.
- Probe: contact on any probe input is contact. No probe input = no contact.

## Switches

- A thread polls all switch inputs, with debounce. A switch acts on the press edge. A switch held at boot does not act.
- The action goes through the command dispatcher, the same as a display command. The same refusal rules apply.
- `start` does not ask "Go to zero first?". That question is only on the display.
- `move zero`: rapid Z to 0, then X to 0, the same as `start zero`. A stop, an ESTOP or a mode start cancels the X move.
- An action can have more than one command, separated by `;`.

## Encoders

- `hw`: a PCNT unit. The spindle always has unit 0. The hw handwheels get the next units, in function order.
- `sw`: interrupts on both edges of A and B. A and B are read once both have been steady for the debounce time, then decoded with a state table. Same count as a PCNT unit. Only for slow encoders.
- Both count the edges of A and B, so contact bounce on one line cancels.
- Each encoder has a filter in µs: the PCNT glitch filter (hw, pulses shorter than it are ignored) or the debounce time (sw, rounded up to the kernel tick). Each function has a default; handwheels get the hw maximum, for mechanical contacts.
- At boot, `Pins` connects each hw encoder's A and B to its PCNT unit through the GPIO matrix and sets its glitch filter. Units that the map does not use get a constant low input.
- The PCNT pins in the board's `pinctrl` are only the routing until `Pins` starts.
- On the nanoels H5 the handwheel connectors have B before A, so the default map has A and B swapped compared with the case labels. Use the handwheel invert-direction setting.

## Storage

- One `PersistBlob` entry (`pins/map`): a list of assignments and the connector names.
- An assignment: function id, connector, start pin, encoder type, glitch filter, signal options. The list has a fixed maximum length.
- If no map is saved, the firmware uses the board defaults.
- An invalid assignment (unknown function id, bad pin, conflict) is ignored and logged as a warning. It stays in the saved map. In a conflict, the first assignment in the list stays.
- If a required function is missing: pins fault. Move, jog and start reply `err pins: <reason>`. The display shows a toast at boot and on each refused command. Settings and the web UI still work.
- Recovery: the `pins reset` serial command saves the board defaults.

## Validation

The firmware (`PinMap::validate`) and the web UI (`validate`, `nameIssues` in `web/client/pins.ts`) check the same rules:

- Required functions have pins.
- Exclusive functions have at most one assignment.
- Every signal is on a GPIO pin that is not reserved.
- No GPIO is used twice.
- Outputs have no pull.
- The encoder type is allowed for the function.
- There are enough PCNT units for the hw encoders.
- Connector names: not too long, no leading or trailing spaces, no control characters.

Strapping pins are a warning only.

## Interfaces

- `GET /pins`: the board and the saved map.
- `GET /pins/functions`: the function table.
- `POST /pins`: validates and saves a map. Only when the machine is idle. Applies after a reboot.
- Serial `pins`: shows if the saved map or the board defaults are in use. `pins reset`: saves the board defaults.

## Web UI

The Pins page loads `/pins`.

- **Connector map** (primary view): the board's connectors and their pins. Reserved pins are locked with a red mark, strapping pins have an amber mark. A line under the map gives the details of the pin under the pointer.
- **Functions:** the required functions and the assigned functions. Each one with its connector and pins, encoder type, invert/pull options, Move and Remove. Required functions have no Remove.
- **Add function:** a list of the functions that can be added: the non-exclusive functions and the unused exclusive functions, in groups.
- **Problems:** errors and warnings.

Assignment:

- Select a function, then a start pin. The map shows where the function fits.
- Or select a free pin, then a function that can start there.

Connector names:

- Click a connector's name to change it. An empty name sets it back to the board's title.
- Two connectors can have the same name.

Highlights:

- Hover (or tap) a function: its pins are highlighted.
- Hover (or tap) a connector or pin: the related functions are highlighted.

Save:

- One "Save and reboot" button, with a confirmation. It is disabled while there are errors.

## Phases

1. Keyed map (backend): a list of assignments, the `exclusive` flag, keyed storage, invalid assignments ignored on load. Multiple probes.
2. Function table from the backend (web): `GET /pins/functions`, the web copy removed, Add and Remove.
3. Pins fault: refused commands and display toast.
4. Switches and `move zero`.
5. Running output.
6. Pin map in the settings backup.

Phases 1 and 2 ship together. Phases 3 to 6 do not depend on each other.

## Left to do

- Test on the lathe: motors, spindle, handwheels (hw and sw), probe, switches, output, and a moved function.
- Recovery from the display: a menu item with a confirmation that saves the board defaults.
- ESTOP input function.
- Printable connector label with the user's names.
- Optional: a live view of each input's level, to help identify wiring.
- Optional: a maximum edge rate for `sw` encoders, so the UI can warn.
