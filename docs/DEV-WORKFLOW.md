# Windows dev workflow — building, running, and testing

A guide for developing on Windows. Two things differ from typical web
development: **there's no hot reload** (edit → compile → run the exe) and
**a toolchain is required** (compiler + Qt), which you set up once.

## How this differs from web dev (short version)

| Web | Here |
|---|---|
| `npm install` | Visual Studio 2022 + Qt 6 (once) |
| `npm run dev`, changes picked up automatically | `cmake --build build` → run the `.exe`. Incremental build after editing one file: 5–30 seconds; first full build: 5–10 minutes |
| Open localhost in a browser | Run `build\apps\device-center\sony-device-center.exe` |
| DevTools / console.log | `qDebug()` / `std::cerr` → the console the exe was launched from; QML errors go there too |
| Jest / Vitest | CTest (`ctest --test-dir build`) — existing tests for protocol, transport, IPC, and the UI controller |
| Deploy | GitHub Actions (`release.yml`) builds the installer; nothing to install locally — the exe runs straight out of `build/` |

Leave the MSI-installed version alone: the dev build lives in `build/` and
the two don't interfere with each other. Just don't run both at once — they
both want the Bluetooth connection to the headphones.

## 1. Toolchain setup (once)

What you likely already have if you're on a typical C++/Qt setup:
- Visual Studio 2022 Community, MSVC 14.35 —
  `C:\Program Files\Microsoft Visual Studio\2022\Community`
- CMake and Ninja, bundled with VS —
  `...\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\{CMake,Ninja}`

What's missing is **Qt 6**. Install it the same way CI does
(`jurplel/install-qt-action` uses `aqtinstall` under the hood), no Qt account
needed:

```powershell
pip install aqtinstall
aqt install-qt windows desktop 6.10.0 win64_msvc2022_64 --outputdir D:\Qt
```

This produces `D:\Qt\6.10.0\msvc2022_64`. The base install includes
everything the app needs (Core, Gui, Qml, Quick, QuickControls2, Shapes).
~1.5 GB.

If `6.10.0` isn't found, list what's available:
`aqt list-qt windows desktop`.

## 2. Building

Everything is wrapped in `scripts\win-dev.ps1` — it sets up the MSVC
environment (via `vcvars64.bat`), puts Qt on `PATH`, and knows about the
quirks below. Run it from a regular PowerShell at the repo root:

```powershell
.\scripts\win-dev.ps1 configure   # once, and after CMakeLists changes / switching branches
.\scripts\win-dev.ps1 build       # incremental build
.\scripts\win-dev.ps1 test        # ctest (you can pass -R <regex>)
.\scripts\win-dev.ps1 run         # build + windeployqt + launch the GUI
.\scripts\win-dev.ps1 ctl info    # build + run sonyctl with arguments
```

What the script does under the hood (in case you need to run things
manually):

- `configure` = `git submodule update --init` + `cmake -B build -G Ninja
  -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH=D:\Qt\6.10.0\msvc2022_64
  -DBUILD_TESTING=ON -DSONY_REQUIRE_QT=ON`. The `Client/imgui` submodule is
  required by the legacy client; configuration fails without it.
- Device images in `Client/resources/devices` are kept under 800 px
  (`python scripts/shrink-device-images.py` after adding new ones): they get
  packed into the exe via `qml.qrc`, and with 1.5 MB originals MSVC used to
  fail with `C1060: out of heap space`.
- The MSVC environment comes from `vcvars64.bat`. `Launch-VsDevShell.ps1`
  doesn't find `vswhere` on some machines and doesn't add `rc.exe` to `PATH`
  — don't use it.

Ninja figures out on its own which files changed. Editing a `.cpp` rebuilds
just that file and relinks the exe. Editing a `.h` rebuilds everything that
includes it. Editing a `.qml` file also needs a rebuild since QML is packed
into the exe via `qml.qrc` (`qrc_qml.cpp` gets regenerated, ~15 s).

`Release` instead of `Debug` is intentional: a Debug build of a Qt app
starts noticeably slower, and the debugger is rarely used. When step-through
debugging is needed, use a separate `build-debug` folder with
`-DCMAKE_BUILD_TYPE=Debug`.

## 3. Running

The exe needs the Qt DLLs available. Two options:

**A. Add Qt to `PATH` for the session** (quick, for development):

```powershell
$env:PATH = "D:\Qt\6.10.0\msvc2022_64\bin;$env:PATH"
.\build\apps\device-center\sony-device-center.exe
```

**B. `windeployqt`** — copies the DLLs next to the exe, producing a
self-contained folder like the installed version (this is also what
`cmake --install` does):

```powershell
D:\Qt\6.10.0\msvc2022_64\bin\windeployqt.exe --qmldir apps\device-center\qml build\apps\device-center\sony-device-center.exe
```

After this the exe can be double-clicked from Explorer. No need to redo it
unless the set of Qt modules used changes.

The app is built as a GUI subsystem target (`WIN32_EXECUTABLE ON`), so it
has no console of its own. To see `qDebug`/`std::cerr` output, launch it
from a terminal — the output goes there. If nothing shows up, try
`$env:QT_LOGGING_RULES="*.debug=true"` temporarily, or use `sonyctl -v` for
protocol-level diagnostics.

The CLI builds alongside it:

```powershell
.\build\apps\sonyctl\sonyctl.exe info
.\build\apps\sonyctl\sonyctl.exe -v battery
```

`sonyd` (the daemon) doesn't work on Windows — IPC isn't implemented there
(`libs/sony-core/src/IpcServer.cpp`). The GUI and `sonyctl` each open
Bluetooth directly, so **they cannot run at the same time**: close the GUI
before running `sonyctl`, and vice versa. This is tracked in the roadmap
(Phase 5).

## 4. Tests

```powershell
.\scripts\win-dev.ps1 test
```

The Qt tests need the Qt DLLs on `PATH` — the script handles that. Running
bare `ctest` without `PATH` set fails
`device-controller-worker-integration` with exit code `0xc0000135` (DLL not
found) — that's not a bug in the test itself.

Coverage: frame codec, V1/V2 protocols against a fake transport (with
literal request/response bytes), the notification dispatcher, IPC (the
Windows portion is skipped), the Qt controller. No headphones are needed for
any of this.

Run a single suite: `.\build\tests\sony-protocol-tests.exe`.

Rule for protocol features: capture real bytes from the XM5 first
(`sonyctl -v <command>` prints a hex dump of the frames), add them to the
tests as a fixture, then write the implementation against that test.

## 5. Feature development cycle

1. Branch: `git checkout -b feat/tray-icon`.
2. Make the change (an agent can do this directly in the editor / via tools).
3. `cmake --build build --parallel` — does it compile? Read compiler errors
   the way you'd read TypeScript errors — they're precise.
4. `ctest --test-dir build` — did anything break?
5. Run the exe and try it by hand with the headphones connected.
   For protocol features, start with `sonyctl` — it's easier to inspect the
   raw bytes there.
6. Commit, open a PR against your fork.

Steps 3–4 can be run by an agent from chat; step 5 (GUI with real
headphones) needs a human, since it requires eyes on the screen and ears on
the audio. A screenshot of the window can be shared in chat instead of a
live look.

## 6. Checking the UI without headphones

```powershell
.\scripts\win-dev.ps1 run --simulated
```

The GUI spins up a built-in WH-1000XM5 simulator in-process (the same one
used by `sonyd --simulated`, implemented in
`libs/sony-core/src/SimulatedDevice.cpp`). It answers requests, applies SET
commands, and sends notifications, so toggles and sliders behave as they
would with real headphones. This is the main mode for UI, tray, and theme
work — real headphones are only needed for protocol features.

The simulator drains 1% every 3 seconds. To have the Battery page and the
remaining-time estimate populated immediately instead of waiting a full day:

```powershell
.\scripts\win-dev.ps1 run --simulated --simulated-history
```

This flag replaces the simulator's battery log with a synthetic week of
usage (draining during the day, charging in the evening). Logs live in
`%LOCALAPPDATA%\SonyBridge\Sony Device Center\battery-history\<address>.json`,
one file per device; the simulator's file
(`CC-98-8B-00-11-22.json`) can be deleted freely.

`SONY_UI_WINDOW=980x660` combined with `SONY_UI_SCREENSHOTS` captures every
page at a given window size — useful for checking the minimum supported
size.

### EQ preset library

`EqualizerLibrary` stores curves in
`%LOCALAPPDATA%\SonyBridge\Sony Device Center\equalizer-presets.json`
(`AppConfigLocation`); the format matches the export format — `{"format":
"sony-device-center-eq", "version": 1, "presets": [{name, clearBass,
bands[5]}]}`, so for screenshots you can just drop a file in place. The
"active" preset is computed from the headphones' current state (custom slot
`0xa0` plus a curve match), not from the last click. The test
`equalizerLibraryStoresAppliesAndImports` runs in a temp folder.

### Global hotkeys

`HotkeyManager` registers key combinations via `RegisterHotKey` without a
window, so `WM_HOTKEY` arrives as a thread message and is caught by
`QAbstractNativeEventFilter`. While the capture field in Settings has focus,
registrations are suspended (`suspend(true)`) — otherwise an already-bound
combination would fire instead of being recorded. To see the card with
assigned shortcuts and a conflict, it's enough to write values into the
registry — `HKCU\Software\SonyBridge\SonyDeviceCenter\hotkeys\<action>` with
a `shortcut` (Qt's portable form, e.g. `Ctrl+Alt+N`) and `enabled`; a
`SONY_UI_SCREENSHOTS` run additionally saves `settings-hotkeys.png` — the
Settings page scrolled to that card. The tests
(`hotkeyBindingsPersistAndParse`, `hotkeysRouteActionsToController`) run in
the `hotkeys-test` group and, on Windows, additionally send `WM_HOTKEY` via
`PostThreadMessage` without touching the keyboard.

### Auto-reconnect on Bluetooth events

`BluetoothWatcher` (Windows only) holds a message-only window and a
`RegisterDeviceNotification` subscription on each radio's handle; an HCI
"connection came up" event triggers
`DeviceCenterController::wakeConnection()` → `DeviceService::wake()` on the
worker thread (resets the backoff, retries on the next tick, i.e. ≤ 0.5 s).
If `BluetoothFindFirstRadio` finds nothing (Bluetooth disabled system-wide),
there's no availability and the regular retry loop takes over; when a radio
comes back, a `DBT_DEVICEARRIVAL` for `GUID_BTHPORT_DEVICE_INTERFACE`
arrives and the watcher rescans. Status is logged via `qInfo` ("Bluetooth
link events on/unavailable", "Bluetooth link up: <address>") — the GUI app
has no console, so use DebugView or run under a debugger to see it. To test
live: turn the headphones off, wait ~30 s (backoff reaches its max), turn
them back on — the app should reconnect within a second or two, not after
half a minute.

## 7. Installer

Not needed locally. `release.yml` in GitHub Actions builds the exe + NSIS
installer and publishes it to the fork's Releases when a `v*` tag is
pushed. If you want to "install it properly" locally,
`cmake --install build --prefix C:\Apps\SonyDeviceCenter` produces a ready
folder with DLLs, no installer required.

## Common problems

- **`Qt6Config.cmake not found`** — `CMAKE_PREFIX_PATH` wasn't passed, or
  it points at the wrong Qt version/path.
- **`cl` / `rc` not found** — run through `scripts\win-dev.ps1`, not
  `Launch-VsDevShell.ps1`.
- **`C1060: compiler is out of heap space`** — oversized images ended up in
  `qml.qrc`; run `scripts/shrink-device-images.py`.
- **`Cannot find source file: imgui/imgui.cpp`** — the submodule wasn't
  fetched: `git submodule update --init --recursive`.
- **The exe launches and immediately closes** — missing Qt DLLs; see
  section 3.
- **White/blank window, console shows `module "QtQuick.Shapes" is not
  installed`** — `windeployqt` was run without `--qmldir`, or a different
  Qt install is ahead of the right one on `PATH`.
- **`Bluetooth: connection refused / device busy`** — a second instance is
  holding the connection (the installed version, `sonyctl`, or the Sound
  Connect app on a phone). Close whichever one isn't needed.
- **Odd build errors after switching branches** — rerun `cmake -B build
  ...`; as a last resort, delete `build/`.
