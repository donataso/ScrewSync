# Hardware

Boards and screens are separate definitions, and a target combines one
board with one screen. Current target: nanoels H5 with a 5" Nextion.

## nanoels H5

Parts: the
[nanoels H5 parts list](https://github.com/kachurovskiy/nanoels/tree/main/h5#parts-list).
Some cables plug into other connectors than on the original firmware. See
[install.md](install.md).

## Additions

All are optional.

| Part                                                                                | What for                                         | Docs                     |
|-------------------------------------------------------------------------------------|--------------------------------------------------|--------------------------|
| Home switches or sensors                                                            | homing, work coordinates, travel limits          | [homing.md](homing.md)   |
| Tailstock sensor                                                                    | shares the X home input                          | [homing.md](homing.md)   |
| [817 optocoupler module](https://www.aliexpress.com/item/1005006377989607.html)     | easier sensor wiring, input for the tailstock    | [homing.md](homing.md)   |
| Probe: contact plate or switch                                                      | touch off Z or X                                 | [probe.md](probe.md)     |

Inputs and outputs go on any free connector pin. See [pins.md](pins.md).
