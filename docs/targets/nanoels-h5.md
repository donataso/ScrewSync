# nanoels H5

Target `nanoels-h5-nextion-5in`: the
[nanoels](https://github.com/kachurovskiy/nanoels) H5 board with its 5"
Nextion display. Parts: the
[nanoels H5 parts list](https://github.com/kachurovskiy/nanoels/tree/main/h5#parts-list).

## Connectors

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

**Why the connections were moved**: the STEP pin of the Z connector is GPIO35, which on
the ESP32-S3R8 is one of the octal PSRAM data lines, bonded inside the chip
package. Driving it as a stepper output corrupts the PSRAM bus, which
breaks Wi-Fi (its driver allocates from PSRAM) with symptoms ranging from
allocation failures to crashes. So the motors, and their handwheels with
them, sit one connector to the right, and there is no Y axis.

## Handwheels

The handwheel connectors have B before A. Thus the default map has A and B
swapped compared with the case labels. If a handwheel turns the wrong way,
toggle `invertHandwheelZ` or `invertHandwheelX`.

## Display

**Flip the display 180 degrees** so its viewing angles are correct.
Alternatively, flash `display_180.tft` from the release on the web client.
