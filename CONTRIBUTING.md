# Contributing to Sony Device Center

Thanks for looking at this fork. This document covers how to get a dev
environment running and the rules that keep contributions mergeable —
especially around the Bluetooth protocol, which is easy to get subtly wrong
in ways that only show up on real hardware.

This is a fork of [marconvcm/sony-device-center](https://github.com/marconvcm/sony-device-center).
General project info, features, and the basic build steps for Linux/macOS/Windows
are in the [README](README.md#-building-from-source). If you're setting up on
Windows and want the full walkthrough — toolchain install, the `scripts\win-dev.ps1`
helper, running without hardware, troubleshooting — see
[docs/DEV-WORKFLOW.md](docs/DEV-WORKFLOW.md).

## Before you start

- [docs/ROADMAP.md](docs/ROADMAP.md) tracks what's planned and in what order,
  including the Phase 3 protocol work (multipoint, playback/volume, button
  behavior, etc.) — worth checking before starting something large, in case
  it's already scoped or partially done.
- [docs/device-matrix.md](docs/device-matrix.md) tracks which devices/opcodes
  are verified on real hardware versus assumed from community reverse-engineering.
- For anything nontrivial, opening an issue first to align on approach saves
  rework — especially for protocol features or UI changes.

## Protocol rules (non-negotiable)

The Bluetooth protocol layer is reverse-engineered and opcodes can mean
different things across protocol generations. Two rules exist specifically
to prevent regressions here:

1. **Community opcode tables are hints, not truth.** Sources like
   Gadgetbridge or SonyHeadphonesClient are a useful starting point, but
   nothing is trusted until it's been replayed against real hardware and
   pinned in a test fixture with the literal bytes.
2. **Never change protocol bytes casually.** As a concrete example: opcode
   `0x22` means POWER OFF on protocol V1 and BATTERY on V2. A test
   (`ProtocolV1Tests`) exists specifically to pin that V1 queries never emit
   `0x22`. Mixing up opcodes between protocol generations is the easiest way
   to send the wrong command to someone's headphones.

New protocol features should go through this pipeline, in order:

```
sonyctl command → verify on hardware with -v (hex dump) →
literal-byte fixture in tests/ → IProtocol / SonyDevice / JsonProtocol →
controller → QML
```

If you can't verify something on real hardware, that's fine — just mark it
clearly as unverified in `docs/device-matrix.md` and the roadmap rather than
presenting it as confirmed behavior. Hardware reports from contributors are
one of the most valuable things you can contribute, even without code
attached — see the [Supported Devices](README.md#-supported-devices) table.

## i18n

UI strings are never hardcoded in QML/C++. Add a key with translations for
all seven supported languages (en, pt_BR, es, de, fr, ja, ru) using:

```bash
python scripts/i18n-add-keys.py new-keys.json
```

then reference it with `window.tr("key")` in QML or `controller.t("key")` in
C++. Brand names (DSEE, Speak-to-Chat, Clear Bass, ANC, LDAC, etc.) stay
untranslated in every language.

## Git workflow

- One topic per branch and PR (`feat/…`, `fix/…`, `chore/…`).
- Commit messages use a conventional prefix; the body explains *why*, not
  just what changed.
- Keep line endings consistent with the repo (LF). If you're generating
  files from Python on Windows, write with `newline="\n"` — the platform
  default otherwise turns the whole file into a diff.
- CI (`.github/workflows/cmake.yml`) runs on PRs targeting `main`.

## Definition of done

1. Builds cleanly and the full test suite passes (`ctest`, Catch2 for libs /
   QtTest for the app — well over a hundred tests at the time of writing).
2. Tests added for new behavior, driven through the simulated device
   (`SimulatedDevice` / `--simulated`), not mocks of the controller.
3. Checked visually with `--simulated` (and an earbuds model too, if battery
   UI is involved); on real hardware if the change touches the protocol.
4. If it's a planned item, `docs/ROADMAP.md` gets ticked off with a one-line
   note of what was decided.

## Questions

Open an issue, or comment on the relevant roadmap item / existing issue if
one already covers the area you're looking at.
