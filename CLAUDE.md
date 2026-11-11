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
- [docs/features.md](docs/features.md) — full feature list
- [docs/probe.md](docs/probe.md) — probe hardware, settings and cycle
