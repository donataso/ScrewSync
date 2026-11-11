# Install

## Hardware

ScrewSync runs on unmodified nanoels H5 hardware, but several cables plug
into different connectors than the labels on the case say.

The table is laid out like the connectors on the back of the case:

|       |             |             |       |         |
|-------|-------------|-------------|-------|---------|
| -     | Z motor     | X motor     | -     | -       |
| PROBE | Z handwheel | X handwheel | -     | -       |
| -     | -           | -           | POWER | ENCODER |

* **PROBE** (optional): - GND SIG -
* **Z motor**: 5V, ENA, DIR, STEP
* **Z handwheel** (optional): 5V, GND, B, A
* **X motor** (optional): 5V, ENA, DIR, STEP
* **X handwheel** (optional): 5V, GND, B, A
* **ENCODER**: 5V, GND, A, B
* **POWER**: 5V, 5V, GND, GND

GPIO numbers of every connector: [pin_map.md](pin_map.md).

Why the motors move: the STEP pin of the Z connector is GPIO35, which on
the ESP32-S3R8 is one of the octal PSRAM data lines, bonded inside the chip
package. Driving it as a stepper output corrupts the PSRAM bus, which
breaks Wi-Fi (its driver allocates from PSRAM) with symptoms ranging from
allocation failures to crashes. So the motors, and their handwheels with
them, sit one connector to the right, and there is no Y axis.

Other hardware notes:

- Flip the display 180 degrees so its viewing angles are correct.
- Probe wiring and probe types: [probe.md](probe.md).

## Firmware

Prebuilt images are in `firmware/`:

| File                | Use                                      |
|---------------------|------------------------------------------|
| `firmware.full.bin` | blank board, written at address 0 by USB |
| `firmware.bin`      | update of an installed ScrewSync         |

Blank board, over USB:

```bash
esptool.py --chip esp32s3 write-flash 0x0 firmware/firmware.full.bin
```

Or without installing anything, from a browser with a web flasher such as
[esptool.spacehuhn.com](https://esptool.spacehuhn.com/): connect, add
`firmware.full.bin` at address 0, and program.

The full image also carries the display UI. Send it to the display once,
either way:

- USB console: `display flash`
- Web client: Device page, "Flash stored display firmware"

Later updates go through the web client's Device page: "Firmware update"
takes `firmware.bin`, and "Display firmware" takes a `.tft`.

Building from source needs a Zephyr workspace and nxwright; the targets are
in the `Makefile`.

## First setup

- The display's Network page turns Wi-Fi on and shows the address of the
  web client.
- Machine settings are on the web client only: lead screw pitch, motor
  steps, speeds, travel and backlash per axis, and the spindle encoder
  resolution.
- -Z is toward the chuck and -X toward the centerline. If an axis moves the
  wrong way, toggle its `invertDirection` setting.
- The spindle angle must increase when the spindle turns forward. If it
  decreases, toggle the `invertEncoder` setting.
- Set the probe polarity (NO/NC) before the first probe. With a contact
  plate, touch the plate to the tool: the contact light must turn green.
