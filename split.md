# Board / display / target split

## Layout

```
boards/<board>/        board.overlay (motor/encoder/probe pins, PCNT, connector node), board.conf
displays/<display>/    nxwright project or LVGL UI, display.cmake (libs it uses)
targets/<target>/      target.overlay (display bus + pins, display partition), target.conf, target.cmake (board, display)
main/                  core
main/lib/              reusable code, including the Nextion link/upload, DisplayImage, Display views
dts/bindings/          screwsync,connector.yaml
```

First target: `nanoels-h5` + `nextion-5in` = `nanoels-h5-nextion-5in`.

## Rules

- `target.cmake` selects which libs go into the build, not the directory.
- `displays/<display>/` holds only what belongs to that one display.
- `lib/Display` view code writes to 5" layout components by name. It stays in `lib` until a second Nextion layout needs different code.
- The 180° rotation is only a `.tft` variant from `display-build`. Same firmware, same target.

## Core ↔ display contract

- `Display.hpp` (`begin`, `connected`, `sendPreview`) is the only interface the core calls.
- `DisplayImage` is Nextion-only. Its use in WebUpdate, Console and CommandSystem goes behind a Kconfig option (for example `CONFIG_SCREWSYNC_DISPLAY_IMAGE`).
- The `DISPLAY_*` constants move from `src/config.hpp` to the Nextion display.

## Build

- Makefile: `TARGET` variable, build dir `.build/<TARGET>`.
- `west build -b <board from target>` with `EXTRA_DTC_OVERLAY_FILE` (board + target) and `EXTRA_CONF_FILE` (board + target).
- `main/CMakeLists.txt` includes `target.cmake`, which adds the display's sources.
- `display-build` builds the nxwright project of the target's display.
- Release artifacts go in a folder per target.

## Steps

1. Split `app.overlay` into board and target, and add the `TARGET` variable. No code change, and the binary should stay the same.
2. Move the display and Nextion sources out of the common source list into `display.cmake`. Move the `DISPLAY_*` config. Move `display/` to `displays/nextion-5in/`.
3. Put the Kconfig guard on DisplayImage use in Web, Console and Command.
4. Make the unit-test CMake follow the source lists.
5. Docs (design, install, screen), only when everything works.

## Later

- Connector binding and runtime pin map (`pin_mapping.md`).
- 7" LVGL target.
