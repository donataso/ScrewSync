# Hardware

Boards and screens are separate definitions, and a target combines one
board with one screen.

## Targets

| Target                                    | Board      | Screen       |
|-------------------------------------------|------------|--------------|
| [nanoels H5](targets/nanoels-h5.md)       | nanoels H5 | 5" Nextion   |

## Additions

All are optional.

| Part                                                                                | What for                                         | Docs                     |
|-------------------------------------------------------------------------------------|--------------------------------------------------|--------------------------|
| Home switches or sensors                                                            | homing, work coordinates, travel limits          | [homing.md](homing.md)   |
| Tailstock sensor                                                                    | shares the X home input                          | [homing.md](homing.md)   |
| [817 optocoupler module](https://www.aliexpress.com/item/1005006377989607.html)     | easier sensor wiring, input for the tailstock    | [homing.md](homing.md)   |
| Probe: contact plate or switch                                                      | touch off Z or X                                 | [probe.md](probe.md)     |

Inputs and outputs go on any free connector pin. See [pins.md](pins.md).
