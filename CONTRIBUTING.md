# Contributing

Contributions are welcome. Open an issue first for large changes.

## Setup

The build setup is not trivial. An AI agent can help you set it up after it
checks the codebase.

Builds are signed with `private/key.pem`, or with MCUboot's default key if
that file does not exist. A board rejects images signed with a different key
than its bootloader's. To change keys, flash `firmware.full.bin` by USB.

## Before you submit

- Run `make lint`. It runs all linters:
  - `clang-format` for C++ style. Config: `.clang-format`.
  - `clang-tidy` for C++ checks. Config: `.clang-tidy`.
  - ESLint for the web code. Config: `web/firmware-client/eslint.config.mjs`.
- Read these configs. They hold the rules. Fix all findings.
- Run `make test`. All tests must pass.
- Test your change on real hardware.

## Tests

Add tests for each new feature and each fix.

## Docs

Update the files in `docs/` when your change affects them.

## Commits

Use a short, one-line commit message. PRs with multiple commits are squashed.

## Releases

1. `git tag -a vX.Y.Z -m vX.Y.Z && git push origin vX.Y.Z`. A tag with `-`
   (`v1.0.0-rc1`) makes a prerelease.
2. CI builds the images, signs them with the `MCUBOOT_SIGNING_KEY` secret,
   and makes a draft release with the commit messages as notes.
3. Edit the notes on GitHub, then publish the draft.

## License

Your contributions are licensed under AGPL-3.0-or-later, the same as the
project.
