# Roadmap — fork development plan

Working plan: what to add, and in what order. Status markers: `[ ]` not
started, `[~]` in progress, `[x]` done. Items that touch the protocol are
marked ⚠ — for those, the rule from `PROMPT.md` applies: verify the device's
actual response via `sonyctl` first, add a fixture with the real bytes to
the tests, and only then change application behavior.

Reference device for verification: **WH-1000XM5** (protocol V2, Windows).
For build/run instructions, see `docs/DEV-WORKFLOW.md`.

The opcodes below are taken from community reverse-engineering of the Sony
protocol (SonyHeadphonesClient / mdr-protocol, etc.). They are not verified
against our hardware until marked "✅ verified".

---

## Phase 0 — quick fixes

- [x] **`--simulated` for the GUI on Windows.** `sonyd --simulated` isn't
  available on Windows (no IPC), and the GUI had no simulator of its own —
  the UI couldn't be checked without headphones. Added a flag that swaps in
  `FakeTransport` inside `DeviceBackend`. Needed for all UI, tray, and theme
  work.
- [x] **Bloated `qrc_qml.cpp`.** 21 MB of PNGs turned into a 110 MB C++
  file, and MSVC ran out of memory. Images shrunk to 800 px
  (`scripts/shrink-device-images.py`); the file dropped to 24 MB, and a full
  build on all cores now takes 111 s with no workarounds.
- [x] **Windows exe icon.** `apps/device-center/CMakeLists.txt` built the
  exe without a resource `.rc`, so `sony-device-center.exe`, the taskbar,
  and the installer shortcut had no icon (the `.ico` was only used for the
  NSIS installer itself). Added `packaging/windows/app.rc` with
  `IDI_ICON1 ICON "sony-device-center.ico"` and wired it into the target
  under `WIN32`.
- [x] **Separate L/R/case battery in the UI.** The data already existed in
  `BatteryState` (`left`, `right`, `caseBattery`); display was deferred (see
  `technical-debt.md`).
- [x] **Slider coalescing.** While a command was in flight, intermediate
  Ambient level / EQ values were dropped. Now the last desired value is
  remembered and sent once the previous operation completes.
- [x] **Power Off from the app** ⚠. V2 — `0x24 0x03 0x01` ✅ verified on
  WH-1000XM5 (2026-09-16). V1 — `0x22 0x00 0x01` per Gadgetbridge, not
  verified on hardware. `sonyctl power off` plus a red button on the
  Overview page. Tray integration ships together with the tray icon
  (Phase 1).

---

## Phase 1 — Desktop conveniences (no protocol changes)

- [x] **Russian UI language.** As part of this, all ~65 strings that were
  hardcoded in QML were pulled out into translation keys — previously only
  the menu and settings were "translated"; now everything is translated
  across all 7 languages.
- [x] **Tray icon** (`QSystemTrayIcon`,
  `apps/device-center/src/TrayController.cpp`). A battery ring with a
  number (green / red <20% / blue while charging / gray when offline), a
  menu with NC / Ambient / Off / Speak-to-Chat / Power Off / Open / Quit,
  window close → minimize to tray (configurable), autostart with
  `--minimized`. The app switched to `QApplication` + QtWidgets for this.
- [x] **System notifications** (`NotificationController`). Low battery
  (10–30% threshold, repeats at 10%), connected/disconnected, charging
  complete; each toggleable in Settings. On Windows these are native WinRT
  toasts (`WindowsToast.cpp`, registering an AppUserModelID in `HKCU`),
  because Windows 11 silently drops
  `QSystemTrayIcon::showMessage` balloon notifications; other platforms use
  the tray balloon.
- [x] **Global hotkeys** (`HotkeyManager`, a "Hotkeys" card in Settings).
  NC↔Ambient, Off, Speak-to-Chat, "Show window"; disabled by default,
  bindings stored in `QSettings`
  (`hotkeys/<action>/{enabled,shortcut}`), triggering routes through
  `DeviceCenterController` the same way the tray menu does, plus a toast
  (a "Hotkey feedback" toggle in notifications). Windows only for now
  (`RegisterHotKey` + `QAbstractNativeEventFilter`); the card reads
  "unavailable" on Linux/macOS. Next steps: Linux/X11 `XGrabKey`, Wayland
  only via a portal.
- [x] **Custom EQ preset library** (`EqualizerLibrary`, a "My presets" row
  on the equalizer page). The headphones have exactly one custom slot; on
  the PC side, any number of named curves can be stored in
  `AppConfigLocation/equalizer-presets.json`. The active pill is
  highlighted only while the slot actually holds that exact curve;
  right-click gives rename / overwrite / export / delete; import and export
  use the same JSON format through `QFileDialog`.
- [x] **Battery history** (`BatteryHistory`, the "Battery" tab). A data
  point on every level/charging change, plus connection markers; JSON, one
  file per device, in `AppLocalDataLocation/battery-history/`, retained for
  180 days. The estimate is based on the current session's drain rate
  (starting from when it was taken off the charger; a brief disconnect or
  app restart at the same level doesn't break the session), shown as "—"
  until there's been at least a 2% drop over at least 5 minutes; surfaced in
  the Overview chip and the tray tooltip. Fixed against a real XM5 log
  (`fix/battery-history-card`): the rate is only computed between two level
  *changes* (connection markers don't count as a percentage boundary), 1%
  and 5 minutes is enough; restarting the app at the same or a plausibly
  lower level (≤15%/h, gap ≤6h) continues the session, while a level
  increase starts a new one (the XM5 doesn't play while charging, so there's
  no charging entry in the log); rates below 1%/h aren't shown. Until an
  estimate exists, the card reads "Not enough data yet" instead of empty
  "•••••". Fixture: a real XM5 log used in `BatteryHistoryTests`. The chart
  is drawn on QML `Canvas` (no QtCharts), 24h / 7-day views. For the
  simulator, see `--simulated-history`. Chose JSON over Qt Sql deliberately
  — avoids a new module and installer plugin.
- [x] **`sonyctl --json`** (`CliRequest` in sony-core). CLI command words
  are turned into the same typed JSON request the GUI sends, and the
  response is printed as a single line
  (`{"version":1,"ok":true,"data":…}` or `error`), exit code 0/1;
  `battery`, `eq get`, `status` print just their own object. This also
  brought `stc`, `adaptive`, `connect`, `disconnect` to the CLI — they go
  through the typed path and work in text mode too. The legacy parser and
  its prose output were left alone (see "Typed CLI output" in
  `technical-debt.md`).
- [x] **Auto-connect when headphones appear** (`BluetoothWatcher` +
  `DeviceService::wake()`). Windows: a message-only window with
  `RegisterDeviceNotification` on each radio catches HCI connection events
  (`GUID_BLUETOOTH_HCI_EVENT`); any connection coming up resets the backoff
  and the next tick tries to connect right away instead of waiting 1…30 s.
  Radios are rescanned as they appear/disappear (Bluetooth toggled off and
  on, a dongle plugged in). The periodic retry loop remains as a fallback,
  and is the only path on Linux/macOS. Upstream PR #53 (Windows: show
  paired-but-not-connected devices) and #52 (Sony-only discovery) are still
  open — not duplicated here.
- [x] **Update check** (`UpdateChecker`, branch `feat/update-check`). On
  launch (after 1.5 s, if "Check for updates on startup" is enabled, which
  it is by default) and via a Settings button — a single GET to
  `api.github.com/repos/chikirao/sony-device-center/releases/latest`
  through `QNetworkAccessManager` with a 5 s timeout; the tag is compared
  against `SONY_DEVICE_CENTER_VERSION` using semver rules (`v` prefix,
  pre-release ranks below the final release, 0.2.1 < 0.2.10). The "About"
  card shows status plus "Download" (the `-win64.msi` / `-macOS.dmg` /
  `-Linux.deb` asset for the current platform) and "Release page", or
  "Check now"; a toast fires on finding a new version if the "New version"
  notification toggle is enabled (once per version). Offline / 403
  rate-limit / 404 all quietly resolve to "Couldn't check". HTTP sits
  behind a `ReleaseFetcher` interface; tests
  (`sony-update-checker-tests`, 14 of them) run against stubbed responses,
  no real network calls. `--simulated-update <version>` injects a canned
  response for screenshots. Also fixed the "About" card overflowing at
  980 px (fact columns now share width and wrap).

---

## Phase 2 — Redesign (done in Codex)

Goal: a new visual language following mockups (dropped into
`docs/design/`): light workspace + black sidebar, large numbers and
headings in a dot-matrix display font, flat cards with a thin border, no
gradients or glows. The current dark theme remains as an alternative.

What the mockups show:
- **Header** on every page: an eyebrow "CONNECTED DEVICE", the model name
  in dot-matrix, a subtitle, three icon chips — CODEC / BATTERY / SOUND MODE.
- **Home:** a large mode card (dot-matrix heading + description + three
  buttons NC / Ambient / Off) with a headphone photo; below it, three cards:
  Battery & Connection (percentage + progress bar, Bluetooth status, codec),
  Current Session (listening time, average volume), Quick Actions (Open
  Equalizer, Device Switcher).
- **Sound Modes:** a full-width segmented control; an Ambient slider 0–20
  with a dot-matrix value; Focus on Voice; an Environment card (Office /
  Low noise / Indoor / Few people); a "Suggested for you" row (Focus /
  Commute / Work / Exercise).
- **Equalizer:** a row of preset pills; 5 vertical bars over a dB grid;
  Clear Bass slider with a dot-matrix value; an Active Preset card with a
  description and a "Save as Custom" button.

Mockup elements that need data we don't have yet (stub out or hide until the
relevant phase lands): Current Session — time and volume (Phase 3,
playback/volume); Environment — on the XM5 this is the phone app's Adaptive
Sound Control, unavailable over the protocol, so leave it decorative or
remove it; Suggested / scenes — "future" phase; Save as Custom — EQ library
(Phase 1, already done).

Technical work:
- [x] **Extract Main.qml into components and pages.** Six pages, a Sidebar,
  and eleven reusable controls; explicit window dependency, unchanged dark
  styling. QML loading checks cover both simulated models, en/ru, and both
  window sizes (upstream #21).
- [x] **Theme singleton.** Dark/light palettes, runtime system theme
  selection, persisted appearance settings, reduced motion, and optional
  MSAA icon smoothing.
- [x] **Fonts.** Manrope (OFL, `assets/fonts/Manrope/`, static weights
  built from the Google Fonts variable font via fontTools) — the primary
  font, includes Cyrillic, registered in `main.cpp` and set as the app
  font; kana falls back to the system font by glyph coverage. Space Grotesk
  was rejected — no Cyrillic support. Dot-matrix text (device name, mode,
  percentages, codec) is drawn by a custom `DotText` component on Canvas
  from a 5×7 table in `qml/DotGlyphs.js`: Latin, digits, Cyrillic,
  punctuation; diacritics are stripped via normalization, everything else
  (kana) falls back to bold Manrope. Glyphs appear left-to-right with a
  scatter across the dots, and neighboring displays start with a small
  delay. Slider values use `DotValue`: clicking the number opens keyboard
  entry. Doto / "Doto RU Draft" weren't needed in the end. **Do not use**
  FontStruct fonts like "Nothing Font (5x7)" (its EULA forbids
  redistribution) or Nothing's proprietary brand fonts (Ndot-55/57,
  NType-82).
- [x] **Logo.** A dot-matrix "S" (20 dots) — `BrandMark.qml`,
  `assets/mark.svg`, `assets/app-icon.svg`; raster icons (png/ico) rebuilt
  from the same coordinates. Rebuild the macOS `.icns` via
  `packaging/generate-icons.sh`.
- [x] **Shell following the mockups.** Dark sidebar + light workspace, a
  shared device header with three chips (codec, battery, mode), flat cards
  with a hairline border, black active buttons, a monochrome palette in
  both themes. Home / Sound Modes / Equalizer / Features rebuilt from the
  mockups; Battery, Devices, Settings ported to the same components. No
  extra letter-spacing anywhere.
- [x] `SONY_UI_SCREENSHOTS=<dir>` — the app walks through every page on its
  own, saves `pageN.png`, and exits (for review without a mouse). Enables a
  basic render loop so animations run even for an occluded window.
- [x] Battery chart on Canvas: the font now comes from
  `Qt.application.font` instead of `sans-serif` — using `sans-serif`
  triggered a full Windows font enumeration on first opening the tab and
  froze the UI for ~0.8 s.
- [x] **Theme selector in Settings**, including `Qt.styleHints.colorScheme`.
- [x] All new strings go through `window.tr` and
  `scripts/i18n-add-keys.py` (verified by grepping QML: only brand names and
  raw values are hardcoded).
- [x] Checked on `--simulated-model WF-1000XM5`, ru, 980×660
  (`SONY_UI_WINDOW=980x660` for the screenshot run). The equalizer's preset
  row is now scrollable, and the bars keep a 320 px minimum. A few rough
  edges remain at the minimum window size, tracked in the backlog: on
  Overview, values in the "Battery & Connection" / "Battery History" chip
  labels get clipped, and the "Close to face" label overlaps the artwork; on
  the Battery page, the X-axis "Now" label overlaps its neighbor.
- [x] "Quick Actions" at 980 px (`fix/pill-button-overflow`): "Open
  Equalizer" overflowed its button. The card got a `Layout.minimumWidth`
  derived from its own buttons (neighboring cards yield — they already
  elide), and the row weights 6/5/4 were converted to fixed pixels
  300/250/200, since a minimum next to a "weight 4" column broke the
  proportions; `PillButton`'s label is now `fillWidth` + elide, so a
  squeezed button truncates its text with an ellipsis instead of
  overflowing.
- [x] Devices page (`fix/device-switcher-layout`): the "Active"/"Connect"
  buttons drifted because the name column was constrained to its implicit
  width (a `Text` without `fillWidth` caps at its preferred width), leaving
  the rest of the row to the right of the button. Now the name and address
  are `fillWidth` + elide, the button is pinned to the right edge, and the
  button column's width is the max implicit width of both labels (via
  hidden `PillButton` "probes", 120 px minimum) — no magic number. Also
  added: the current device gets its own card, the rest are listed
  separately, an empty state, a "Refresh" button (finally using
  `refreshDiscoveredDevices`), and a `Flickable` for long lists; the
  simulator now seeds two "paired" devices so there's something to look at
  in the list.

Upstream targets the dark theme and Linux; the redesign is fork-specific —
only the component extraction and `Theme` singleton are offered upstream,
if the maintainer wants them.

---

## Device Hub — quick panel for every Bluetooth device

A compact panel off the tray icon, in the spirit of "Bluetooth Battery
Level" (Workstation Master) but in this app's own visual language: every
paired Bluetooth device on one list, charge as a dot-matrix number, and the
Sony set's quick controls inline. Three PRs, stacked.

- [x] **System peripherals behind an interface** (`feat/peripherals-source`).
  `IPeripheralSource` (`refresh()`, `changed()`, a list of
  `Peripheral {address, name, kind, connected, battery, L/R/case}`) with
  three implementations: `WindowsPeripheralSource` (C++/WinRT enumeration
  of paired classic + LE endpoints with `System.Devices.Aep.IsConnected`;
  battery from `DEVPKEY_Bluetooth_Battery` on the Hands-Free device node,
  linked by container id, and from GATT `0x180F` for connected LE devices;
  device class from CoD major/minor and GAP appearance, name heuristics as
  a fallback; polled every 30 s on a worker thread, plus a debounced re-read
  after every `BluetoothWatcher::connectionChanged`), `NullPeripheralSource`
  for Linux/macOS, `FakePeripheralSource` for tests and `--simulated` (a
  mouse at 50 %, a keyboard at 100 %, an idle DualSense at 90 %).
  `PeripheralModel` merges the controller's Sony sets (paired list plus the
  live state of the connected one) with the OS rows by address, Sony side
  winning; connected first, the active set on top, Sony before the rest,
  then by name; incremental updates, never a reset. Decided along the way:
  the stock `GetDeviceSelectorFromPairingState(true)` selectors must be
  used verbatim — a hand-written `ProtocolId AND IsPaired` query makes
  Windows run a 30-second inquiry before answering. Checked on the XM5
  (90 % over HFP), a DualShock and a
  Bluetooth speaker; `SONY_PERIPHERALS_LOG=<file>` dumps every scan.
  11 QtTest cases in `sony-peripheral-tests`, no WinRT involved.
- [x] **The hub window** (`feat/device-hub`). `qml/Hub.qml` + `HubWindow`:
  a frameless `Qt.Tool | WindowStaysOnTop` window created in the main
  window's engine (so it shares `controller`, `peripherals`, `Theme` and
  borrows Main.qml's icons and `tr` through a `mainWindow` context
  property), 360 px wide, 56 px rows, three to six of them (the list
  scrolls past six), placed by `QSystemTrayIcon::geometry()` with the
  cursor as fallback and clamped into the screen's available area
  (`HubWindow::placeNear`, pure and unit-tested for bottom / top / left
  taskbars), slide+fade through `Theme.motionEnabled`, closed by Esc or by
  losing focus — but only once it has actually held focus, and a tray click
  within 350 ms of such a close is the click that closed it, so it does not
  reopen. Row: class tile (new `earbuds` / `mouse` / `gamepad` /
  `noiseOff` glyphs in Main.qml), name, status ("Connected · LDAC · ANC"
  for the active Sony set, "Connected" / "Not connected" otherwise), a
  dot-matrix percentage (hidden when the OS has no reading; earbuds get
  "L 81 R 79 CASE 64" inline — three percent signs would have eaten the
  name column), and for the active Sony set a glyph segment
  NC / Ambient / Off plus power through the controller's existing
  invokables. Sony rows open the main window (Overview for the active set,
  Devices for the rest); footer: "Open Sony Device Center" and a gear to
  Settings; an empty state when nothing is paired. Tray: left click
  toggles the hub, double click opens the main window (and sends the hub
  away first), right click is the menu as before. Screenshots:
  `SONY_UI_SCREENSHOTS` saves `hub.png` from the app and
  `hub-<model>-<lang>-{light,dark}.png` plus `hub-empty.png` from the UI
  tests. Decided along the way: ListView delegates are visual, not
  QObject, children, so tests walk `childItems()`; the UI test's CTest
  timeout went 60 → 150 s.
- [x] **Hub settings, Sony-only filter, optional per-device tray icons**
  (`feat/tray-per-device`). Starts with upstream PR #52 cherry-picked
  (`SonyDeviceDiscovery`: a paired device counts as Sony when its address
  prefix is in the IEEE OUI table for Sony or its name carries a model
  prefix) — before it, every paired Bluetooth device (a Yandex speaker,
  say) landed on the Devices page and among the "Sony" rows. `HubSettings`
  (QSettings group `hub/`, exposed as `hubSettings`): `showSystemDevices`
  (hub lists everything the OS reports, or Sony sets only — OS rows with a
  Sony OUI/name stay either way), `pollIntervalSeconds` (15/30/60/120),
  `trayClickAction` (hub or main window), `trayMode` (`single`: the one
  icon following the connected Sony set, as always; `perDevice`: that plus
  one icon per address in `trayDevices`). The per-device icons are the
  same ring and number with a class badge in the corner drawn with
  QPainter primitives (no QtSvg), tooltip = name and charge, click = hub;
  chosen with a pin that appears on hub rows in per-device mode and with
  switches in the new Settings card "Device Hub", which also states that
  Windows parks new icons in the overflow until dragged out. Tests: 137 →
  143 (upstream's 5 filter cases, `sonyOnlyKeepsSonyLookingSystemRows`,
  `hubSettingsPersistAndValidate`, `trayIconsFollowHubSettings` — the last
  skips without a system tray).

---

## Phase 3 — Protocol V2 features ⚠

Ordered by value to an XM5 owner. For each item: `sonyctl` command →
verify on hardware → fixture → `IProtocol` + `DeviceState` +
`DeviceEventDispatcher` (notifications) → IPC JSON → UI.

### 3.1 Multipoint / connection management ⭐⭐⭐
- [ ] List of paired devices with names and "connected" status.
- [ ] Connect / disconnect a specific device (switch the source between
  phone and PC).
- [ ] Enable pairing mode.
- Commands: `0x36` GET / `0x37` RET / `0x38` SET / `0x39` NTFY (PERI_*);
  types: pairing device management (`0x00`/`0x01`), source switch (`0x02`).
- `0x39` notifications need to be parsed in `DeviceEventDispatcher`.

### 3.2 Playback, Now Playing, volume ⭐⭐⭐
- [ ] Play / Pause / Next / Prev.
- [ ] Track metadata (title, artist, album, status).
- [ ] Headphone volume (read/write).
- Commands: `0xa2` GET / `0xa3` RET / `0xa4` SET / `0xa5` NTFY;
  types: `0x01` playback controller, `0x20` music volume.
  Play=1, Pause=2, Next=3, Prev=4.
- `0xa5` notifications → `DeviceEventDispatcher`, plus tray display.

### 3.3 Button behavior ⭐⭐⭐
- [ ] Which modes the NC/AMB button cycles through (NC↔Ambient,
  NC↔Ambient↔Off).
- [ ] Button assignment: Ambient control vs. voice assistant vs. Quick
  Access (Spotify Tap).
- Commands: `0xf6`/`0xf7`/`0xf8` (SYSTEM_*), type ASSIGNABLE_SETTINGS
  (presumed `0x05`), QUICK_ACCESS (`0x0d`). Exact values and payload format
  need verification on the XM5.

### 3.4 Wearing detection ⭐⭐
- [ ] Pause on removal / resume on wearing.
- Commands: `0xf6`/`0xf8`, type CONTROL_BY_WEARING (`0x02`).
- `DeviceCapabilities` already has a `wearSensor` flag that's unused.

### 3.5 Connection priority ⭐⭐
- [ ] "Sound quality priority" vs. "connection stability".
- Commands: `0xe6`/`0xe7`/`0xe8` (AUDIO_*), type CONNECTION_MODE (`0x00`) —
  adjacent to DSEE (`0x01`), which is already implemented.

### 3.6 Voice guidance ⭐⭐
- [ ] On/off, guidance volume.
- Commands: `0x46` GET / `0x47` RET / `0x48` SET (VOICE_GUIDANCE_*).

### 3.7 Background Music Effect / listening mode ⭐
- [ ] "Room / Living room / Cafe" modes — if the XM5 firmware actually
  exposes them.
- Commands: `0xe6`/`0xe8`, type BGM_MODE (`0x02`).

### 3.8 Battery Care / safe charging ⭐
- [ ] Charge level cap.
- Commands: `0x22`/`0x24`, type BATTERY_SAFE_MODE (`0x08`).

### 3.9 Miscellaneous (low priority / unclear support)
- [ ] Safe Listening (sound pressure monitoring).
- [ ] Head gesture (nod to answer a call) — `0xf6`, type `0x0b`.
- [ ] Sidetone / "hear your own voice during a call" — CALL_SETTINGS.
- [ ] Double-check that `0xf6 0x0a` is really Adaptive Volume: in known
  tables, `0x0a` under SYSTEM_* is CALL_SETTINGS. The XM5's companion app
  doesn't expose an "Adaptive Volume" feature at all.

---

## Phase 4 — Protocol gaps on other devices ⚠

- [ ] **V1 (XM3/XM4): DSEE and Auto Power-Off.** Devices respond to
  `0xe6 0x02` and `0xf6 0x04`, but the responses aren't decoded yet
  (`device-matrix.md`).
- [ ] **XM6: 10-band EQ.** Already done in upstream PR #44 (Cyrus7):
  inquired type `0x04`, 10 bands without Clear Bass, verified on real XM6
  hardware. Don't reimplement — wait for the merge and pull it in.
- [ ] **Profiles in `DeviceProfileRegistry`** for new features: don't
  assume every V2 device supports multipoint / playback / BGM.

---

## Phase 5 — Windows architecture

- [ ] **IPC on Windows.** `IpcServer.cpp` throws "IPC not supported on
  Windows; use direct mode", so there's no `sonyd` on Windows, and the GUI
  and `sonyctl` can't run at the same time (both want RFCOMM). Implement a
  transport over named pipes (or localhost TCP with a token) using the same
  JSON envelope. Would enable: daemon + CLI + GUI + tray simultaneously,
  scripting, hotkeys without the GUI running.
- [ ] **Service / autostart for the daemon on Windows** (Task Scheduler or
  launch from the tray).
- [ ] **Per-device lock** on RFCOMM so two processes don't fight over the
  connection (upstream issue #29).

---

## Upstream (marconvcm/sony-device-center)

The maintainer actively merges external PRs (9 merged over Sept 11–13,
often same-day). No formal rules exist beyond `PROMPT.md`. Contributions go
back in pieces, from least to most opinionated; each PR is one topic,
branched from upstream `main`, commits cherry-picked from the fork:

The order is dictated by tests: nearly everything is verified through
`SimulatedDevice`, so the simulator goes back first, and everything else
follows once it's merged.

1. [x] Exe icon — upstream PR #60 (branch `upstream/win-exe-icon`).
2. [x] `--simulated` for the GUI + the shared simulator — upstream PR #61
   (`upstream/gui-simulated-mode`). Everything else waits on this merging.
   Plus upstream PR #62 (`upstream/core-test-timeout`) — a fix for a flaky
   macOS test timeout that was turning #60/#61 red on macOS.
3. [ ] Power Off (V2 verified on XM5, V1 per Gadgetbridge).
4. [ ] Slider coalescing.
5. [ ] L/R/case battery.
6. [ ] Full i18n (all strings as keys) + Russian.
7. [ ] Image shrinking — separately, with a writeup of the C1060 issue.

Mechanics: `git fetch upstream`, branch `upstream/<topic>` off
`upstream/main`, `git cherry-pick -n <sha>` from the fork's `main`, strip
out our docs/icon, temporarily drop in the shrunk images for a local build
(`git checkout main -- Client/resources/devices`, don't add to the commit —
MSVC runs out of memory on the upstream images), build with a copy of
`scripts/win-dev.ps1` from `%TEMP%`, restore the images, commit,
`git push -u origin`, `gh pr create --repo marconvcm/sony-device-center
--head chikirao:<branch>`. Upstream CI waits for maintainer approval; run
our own CI on the branch beforehand as proof it's green.

Before offering up tray/themes/multipoint, ask in an issue whether the
maintainer wants it upstream. Issue #21 (split up `Main.qml`) is the
maintainer's own idea and our prerequisite for theming — coordinate so the
work isn't duplicated in parallel.

**PR format** — matching merged PRs #36 / #37 / #54 (all from Claude Code,
so the maintainer is used to this style):
- Conventional-commit title: `feat(ui): …`, `fix(protocol): …`.
- First paragraph: what was wrong and what's true now, no preamble.
- `## What changed` — bullet points, with protocol bytes / file names.
- `## Verified on hardware` — model, firmware, OS, exactly what was
  checked (a command → readback table, for protocol changes).
- `## Tests` — test count before/after, what was added.
- `## Not covered` — honestly, what wasn't verified (V1, macOS, etc.).
- `Fixes #N` if it closes an issue; Claude Code footer.
- One PR, one topic; note in the description whether it depends on other
  open PRs.

## "Future" phase — ideas deliberately deferred

- **Profiles / scenes.** A named bundle: NC mode + Ambient level + EQ +
  DSEE + Speak-to-Chat + APO; applied with one click from the window or the
  tray; stored as JSON in `AppConfigLocation`. Shown on the redesign
  mockups as a "Suggested for you" row (Focus / Commute / Work / Exercise).
- **Scheduling/automation** (a scene triggered by time of day or the
  focused app).

## Small notes for upstream (collected along the way)

- The test `Lifecycle prefers connected candidates and honors explicit
  selection` takes ~9 s against CTest's 10 s timeout — it occasionally times
  out on macOS CI. Either raise the timeout or speed it up (it waits on
  real retry delays).

## Working order

1. ~~Phase 0~~ — done, merged into the fork's `main`.
2. ~~Phase 1~~ — done: Russian, tray, notifications, battery history,
   hotkeys (#9), EQ library (#10), `sonyctl --json` (#11), auto-connect
   (#12).
3. Phase 2: redesign in Codex (component extraction and `Theme` first).
4. Phase 3: multipoint and playback — start with `sonyctl`, verify on the
   XM5, add fixtures, then build the UI. Buttons, wearing detection, and
   connection mode follow.
5. Phase 5: named-pipe IPC — once enough features accumulate that need a
   background daemon.

Each feature gets its own branch and PR; protocol ones ship with a fixture
built from real bytes in `tests/`.
