# ScrewSync

Custom firmware for the nanoels H5 electronic leadscrew hardware (ESP32-S3
main controller + Nextion display). Replaces the original Arduino/ESP32
firmware with a Zephyr main controller, an nxwright-authored Nextion UI, and
a web client.

- [docs/design.md](docs/design.md) — architecture, hardware, licensing
- [docs/protocol.md](docs/protocol.md) — command protocol (serial/display/web)
- [docs/screen.md](docs/screen.md) — display UI design and tooling
- [docs/manual.md](docs/manual.md) — operator manual
- [docs/install.md](docs/install.md) — connectors, flashing, first setup
- [docs/hardware.md](docs/hardware.md) — boards, screens, parts and additions
- [docs/features.md](docs/features.md) — full feature list
- [docs/homing.md](docs/homing.md) — homing cycle, sensors and wiring
- [docs/probe.md](docs/probe.md) — probe hardware, settings and cycle
- [docs/calibration.md](docs/calibration.md) — calibration checks in the web client
