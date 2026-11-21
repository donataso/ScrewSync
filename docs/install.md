# Install

## Hardware

Wire the board as its target doc says:

- [nanoels H5](targets/nanoels-h5.md)

Parts and optional additions: [hardware.md](hardware.md). Probe wiring
and probe types: [probe.md](probe.md).

## Firmware

Download the prebuilt images from the
[latest release](https://github.com/donataso/ScrewSync/releases/latest):

| File                | Use                                      |
|---------------------|------------------------------------------|
| `firmware.full.bin` | blank board, written at address 0 by USB |
| `firmware.bin`      | update of an installed ScrewSync         |
| `display.tft`       | display UI                               |
| `display_180.tft`   | display UI, turned 180 degrees           |
| `*.HMI`             | Nextion Editor projects of the display   |

Blank board, over USB:

```bash
esptool.py --chip esp32s3 write-flash 0x0 firmware.full.bin
```

Or without installing anything, from a browser with a web flasher such as
[esptool.spacehuhn.com](https://esptool.spacehuhn.com/): connect, add
`firmware.full.bin` at address 0, and program.

The full image also carries the display UI. Send it to the display once,
either way:

- USB console: `display flash`
- Web client: flash the stored display firmware

Later updates go through the web client: `firmware.bin` for the
controller, a `.tft` for the display.

Building from source needs a Zephyr workspace and nxwright. Each target
(`targets/<target>/`) has its own make rules, such as `<target>-build`
and `<target>-flash`. See [design.md](design.md).

## First setup

- Turn Wi-Fi on at the display. It shows the address of the web client.
- Machine settings are on the web client only: lead screw pitch, motor
  steps, speeds, travel and backlash per axis, and the spindle encoder
  resolution.
- Each axis has separate acceleration and deceleration settings. Friction
  helps the motor stop, so deceleration can be higher than acceleration.
- Speeds are in mm/min and accelerations in mm/s². With inch units, they
  are in in/min and in/s².
- -Z is toward the chuck and -X toward the centerline. If an axis moves the
  wrong way, toggle its `invertDirection` setting.
- The spindle angle must increase when the spindle turns forward. If it
  decreases, toggle the `invertEncoder` setting.
- Turn off the features that the machine does not have (X motor, homing,
  probe) on the display or the web client. A change applies after a
  reboot. See [manual.md](manual.md#features).
- Set "invert" of the Probe pin so that contact is active. With a contact
  plate, touch the plate to the tool: the display must show contact.

If the controller does not start: [troubleshooting.md](troubleshooting.md).
