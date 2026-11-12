# Contributing

Contributions are welcome. Open an issue first for large changes.

## Setup

The build setup is not trivial. An AI agent can help you set it up after it
checks the codebase.

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

## License

Your contributions are licensed under AGPL-3.0-or-later, the same as the
project.
