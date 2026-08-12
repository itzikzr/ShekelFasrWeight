# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## What this is

A Tkinter desktop weighing station for a Swan scale head over RS-232 Serial, cross-platform (Windows + Linux). The conveyor/scale runs continuously; when a weight crosses a threshold the app averages the incoming stream and, once the item leaves the scale, records the settled weight — classified green/red/yellow against a selected product's target weight and tolerances. Every weighing and every product is persisted to a local SQLite database.

`scale_sampler.py` is a thin entry point; all logic lives in the `scale_app/` package.

## Running

```bash
pip install pyserial
python scale_sampler.py
```

Windows: double-click / run `run.bat`. Linux: `./run.sh` (both install `pyserial` if missing; `run.sh` also checks for the `tkinter` system package, which isn't pip-installable — e.g. `sudo apt install python3-tk` on Debian/Ubuntu, `sudo dnf install python3-tkinter` on Fedora).

`openpyxl` is an optional dependency (also auto-installed by `run.bat`/`run.sh`), needed only for the "ייצוא לאקסל" button in `weighing_detail_window.py`; everything else runs fine without it, and that one button just shows an error message pointing at `pip install openpyxl` if it's missing.

There is no test suite, linter, or build step — verification is manual: run the app against a real or simulated Swan head, or exercise `scale_app.main_window.MainWindow` headlessly (`root.withdraw()`) and call `_on_session_done(...)` directly with synthetic readings to drive the decide→classify→DB pipeline without hardware (this is how the current implementation was verified).

Two files are gitignored, local machine state — never commit them:
- `scale_sampler_config.json` (port, baud, thresholds, last-selected product) — loaded on startup, saved whenever a setting changes.
- `scale_data.db` (SQLite — all products and all weighings).

## Architecture

```
scale_app/
  swan.py              parse_swan_frame / parse_swan_frame_bytes — pure frame parsers
  engine.py             WeighingEngine — serial I/O + every background thread
  db.py                 SQLite schema + CRUD (products, weighings) + classify()
  theme.py              cross-platform ttk styling, fonts, window-maximize helper, light/dark palettes
  widgets.py            TrafficLight, StatusPill canvas widgets
  main_window.py        dashboard — the only place that touches both Tk and the DB
  settings_window.py    Connection / Weighing / Diagnostics tabs + calibration wizard
  products_window.py    product CRUD screen
  history_window.py     full weighing log, filters, CSV export
  weighing_detail_window.py   per-weighing raw-reading detail (double-click a row)
  scale_config_window.py     direct Swan SC device commands (Swan_SC_Protocol.md)
```

### Threading contract (extended from the single-file version)

`WeighingEngine` (`engine.py`) owns the `pyserial` connection and runs every background thread — none of them ever touch Tk. They only push `(kind, payload)` tuples onto a `queue.Queue`. `MainWindow._pump_queue` (`main_window.py`), scheduled via `root.after(40, ...)`, is the **only** place that drains the queue, touches widgets, **and writes to SQLite**. This extends the original rule ("threads never touch Tk") to also cover the database, so no `sqlite3` connection ever needs `check_same_thread=False` or locking.

This matters specifically for verdict classification: the continuous monitor thread (`WeighingEngine._monitor_loop`) runs for the entire life of the connection, spanning many weighing sessions, so the operator can change the selected product between sessions. The engine thread therefore reports only the raw decided result (`"session_done"`: readings, elapsed, wall-clock start/end) — `MainWindow._on_session_done` reads whichever product is *currently* selected, calls `db.classify()`, and writes the row.

Threshold/debounce/listen-mode/show-each are **not** Tk vars read once at thread start — they live in `EngineSettings` (plain attributes), written by the main thread when Settings is saved and read every loop tick by the engine thread. This is a deliberate, narrow exception that lets an operator tune the trigger threshold live without reconnecting; it's safe only because the values are simple floats/bools (atomic under the GIL), never Tk objects.

`engine.decide_weight()` is the same flat-window algorithm as before (slides a window of 6 readings, picks the lowest-stdev window, returns its median) — now a standalone function reused by the monitor loop and both diagnostics threads. It returns `(decided, basis, window_start, window_end)` — the last two are the `[start, end)` index range into `readings` that was actually used, so callers can mark which raw readings fed the decision (see below).

**Candidate readings are restricted to `> PEAK_FRACTION_FLOOR` (50%) of the session's peak weight before the flat-window search runs.** Without this, a short dwell time (item pulled off the scale quickly after settling) can make the quiet/empty tail *after* the item was removed statistically flatter than the brief real plateau — the old unrestricted search would then confidently "decide" ~0 instead of the actual weight. This was found and fixed from real exported weighing data (sessions with a long dwell decided correctly either way; short-dwell sessions decided ~0 before this fix and the correct weight after). Falls back to the median of whatever candidates exist (or all readings, if none clear the floor) when fewer than 6 readings qualify.

### Scale device configuration (`scale_config_window.py`, separate from app Settings)

`Swan_SC_Protocol.md` (repo root) is the authoritative reference for the scale's own direct single-character commands (`F`/`B`/`R`/`Q` query-then-set-digit menus, `Z`/`T`/`C` actions, `I`/`V`/`A` read-only queries) — this is distinct from the app-level `SettingsWindow`, which only covers *this app's* connection/threshold settings, never the scale's own NVRAM. `WeighingEngine` exposes one method per command (`query_full_scale`/`set_full_scale`, `zero`, `get_info`, …), each spawning its own thread guarded by `self._device_busy` so two device commands can never race on the port; all share the `_read_until()` helper (also used by calibration). Each parameter row in `ScaleConfigWindow` has its own "קרא" (read) button calling its query method directly — nothing is queried automatically on open or in bulk; the user reads whichever parameter they want, whenever they want, one at a time (`_run()` just sets busy and fires the one command).

Two destructive paths are behind explicit confirmation, per user sign-off before this feature was built: **baud-rate change** (`set_baud`) reboots the scale and silently strands the app at the old baud until the operator reconnects at the new one in `SettingsWindow` — gated by a `messagebox.askyesno` warning before sending. **Factory reset** (`factory_reset`, ESC D) wipes all scale NVRAM (calibration, full scale, baud, everything) — gated by requiring the literal text `RESET` typed into a field before the button even enables, *plus* a confirmation dialog. `factory_reset()` emits `"device_reset_disconnect"` afterward, handled by `MainWindow` exactly like calibration's `"calib_disconnect"`.

**ESC-terminator case note**: `Swan_SC_Protocol.md` documents ESC sequences ending in uppercase `E` (`ESC D ESC E`), but the calibration wizard's already-hardware-verified code (predating this doc) uses lowercase `e` (`\x1B\x65`). `factory_reset()` deliberately matches the working calibration code's lowercase terminator, not the doc — if a future protocol update confirms uppercase is actually required, verify against real hardware before "fixing" this back.

**`pause_device_access()` / `resume_after_device_access()`, not raw `stop_monitor()`/`start_monitor()`**: both `ScaleConfigWindow` and the calibration dialog must silence the scale's actual continuous broadcast (send `s`) before talking to it in request/response mode, not just stop the app's own read loop — in listen mode the scale keeps streaming weight packets regardless of whether anything in the app is reading them, and those stray packets interleave with command responses and defeat `_read_until()`'s idle-timeout detection (it never sees a quiet gap, so every command runs to its full multi-second timeout instead of returning as soon as the real response arrives). `pause_device_access()` also `.join()`s the monitor thread (via `stop_monitor(wait=True)`) instead of just flipping a flag, closing a race where the monitor could still be mid-read when device commands start writing to the same port. `resume_after_device_access()` resends `S` (if listen mode) and restarts the monitor. Any future screen that sends direct scale commands must pause/resume through these, not `stop_monitor()`/`start_monitor()` directly.

### Diagnostics live under Settings, not the main screen

The old one-shot "sample for N seconds" and "LIVE" modes are diagnostic tools for verifying wiring/protocol, not part of normal operation — they're on the **Diagnostics** tab of `SettingsWindow`, backed by `WeighingEngine.sample_once()` / `start_live()`/`stop_live()`. The Swan PC0035 calibration wizard (`WeighingEngine.start_calibration()` + `CalibrationDialog` in `settings_window.py`) is also there. Opening the calibration dialog calls `stop_monitor()` first (calibration and continuous monitoring can't share the port at once) and resumes monitoring on cancel/error; a successful calibration ends with the scale rebooting, which the engine signals via a `"calib_disconnect"` event that `MainWindow` turns into a real `disconnect()`.

### Database (SQLite, stdlib `sqlite3`, `db.py`)

Two tables: `products` (name, target_weight, tolerance_upper, tolerance_lower) and `weighings` (one row per completed session — decided weight, a **snapshot** of the product's name/target/tolerances at the time, verdict, reading count, elapsed, min/max, decision basis). The snapshot means history stays meaningful even if a product is later renamed or deleted.

`db.classify(weight, target, tolerance_upper, tolerance_lower)` is the traffic-light rule: `red` if `weight > target + tolerance_upper`, `yellow` if `weight < target - tolerance_lower`, else `green`. A weighing recorded with no product selected gets verdict `"none"`.

A third table, `weighing_readings`, stores the raw `(t_offset, weight, in_window)` samples behind each weighing so `WeighingDetailWindow` can show them on double-click — `in_window=1` marks the readings inside the flat window `decide_weight()` actually used, rendered in green. Storing every raw reading forever would grow unbounded, so `db.insert_weighing_readings()` prunes down to the `db.DETAIL_RETENTION_COUNT` (100) most recent weighings-with-detail after every insert; the aggregate `weighings` row is never pruned, only its raw-reading backing data — so old entries in the recent list / History still show correctly, just without a detail view (`WeighingDetailWindow` shows a message instead of an empty table in that case).

**IMPORTANT — test isolation footgun**: `db.connect(db_path=None)` falls back to the module-level `DB_FILE` *read inside the function body*, specifically so that reassigning `db.DB_FILE` (e.g. in tests) works. Do not change this back to a `db_path=DB_FILE` default — a default argument is evaluated once at import time, so a default bound that way silently ignores any later reassignment of `DB_FILE` and a parameterless `db.connect()` call (e.g. from `MainWindow.__init__`) keeps hitting the real project `scale_data.db` even when a test believes it redirected it. When testing this app headlessly, prefer passing `db_path=` explicitly (and monkeypatching `db.connect` itself if code under test calls the parameterless form) over reassigning `db.DB_FILE` and hoping.

### Light/dark mode

`theme.py` holds two color dicts (`_LIGHT`/`_DARK`) and mutates its own module-level color constants (`BG`, `CARD_BG`, `TEXT`, …) via `_apply_palette(mode)`. `theme.set_mode(root, mode)` swaps the palette and re-runs `apply_theme(root)`, which re-configures the shared `ttk.Style()` — that alone is enough to live-update every ttk widget, since they all read colors from the style rather than fixed instance options.

It is **not** enough for plain `tk.Label`/`tk.Canvas`/`ScrolledText` widgets (the big weight readout, `TrafficLight`, `StatusPill`, log boxes) or for `ttk.Label`/`Treeview` instances that were given an explicit per-instance `foreground=`/tag color at creation — those bake the color in and never re-read the style. Every screen that owns such a widget therefore exposes a `refresh_theme()` method that reconfigures them from the (now-current) `theme.*` constants; `MainWindow._toggle_theme()` calls its own `_refresh_theme_widgets()` and then `refresh_theme()` on whichever of `settings_window`/`products_window`/`history_window` are currently open (each of those, in turn, forwards to any of their own sub-dialogs, e.g. `SettingsWindow.refresh_theme()` → `CalibrationDialog.refresh_theme()`). When adding a new raw-tk widget or a colored Treeview tag anywhere, add it to the owning screen's `refresh_theme()` too, or it will silently keep the old palette's color after a mode switch. `TrafficLight._lit_color()` re-reads `theme.RED/AMBER/GREEN` on every call (not a cached dict) specifically so the *currently lit* lamp re-colors correctly on switch, not just newly-lit ones. `dark_mode` persists in `scale_sampler_config.json` and is applied before `_build_ui()` runs (via `theme.set_mode`), so the app opens in the last-used mode.

## Protocol notes (see also the module docstring in `scale_app/swan.py`)

- **Swan weight reading (poll mode)**: send a bare `W` (no `\r\n`; a trailing `\r\n` gets interpreted as two separate commands and causes a double response) and read back `<+/-><7 chars><CR>`. The 7 characters are digits with an optional single `.` in any position (or none) — `parse_swan_frame` does not assume a fixed decimal position; if there's no `.` the value is an integer scaled by `/1000`. This is what real Swan heads send in the field — don't tighten it back to a fixed-position regex.
- **Swan continuous streaming (listen mode)**: `WeighingEngine.connect()` sends a single uppercase `S` right after opening the port when `settings.listen` is true, to start the head's continuous broadcast; `disconnect()` sends a single lowercase `s` to stop it before closing. This is tracked via `self._stream_started` (set from `settings.listen` at connect time) rather than re-checking `settings.listen` at disconnect time, so toggling listen/poll mid-session while connected can't leave the head stuck mid-stream.
- **Swan calibration (PC0035)**: a distinct ESC-command protocol (`\x1B\x50`/`\x1B\x4E`/`\x1B\x65`, i.e. ESC P / ESC N / ESC e), only ever used by the calibration wizard.
- Reference docs: `Swan/PC0034.docx` (weight/binary frame) and `Swan/PC0035.docx` (calibration).

## Cross-platform notes

- `theme.maximize(root)` tries `root.state("zoomed")` (Windows), then `root.attributes("-zoomed", True)` (Linux/X11), then falls back to manual geometry from `winfo_screenwidth/height` — don't call `root.state("zoomed")` directly elsewhere.
- `theme.pick_font()` checks `tkfont.families()` at runtime before using a font name, so Windows-only fonts (`Consolas`) fall back to Linux equivalents (`DejaVu Sans Mono`) instead of silently substituting something inconsistent.
- `engine.list_serial_ports()` (wrapping `serial.tools.list_ports`) already returns the right thing on both platforms (`COMx` on Windows, `/dev/ttyUSB0`/`/dev/ttyACM0` on Linux) — no platform branching needed there.

## Repo layout notes

- `M6ConveyorAI` is a git submodule (see `.gitmodules`) left over from when this tool also supported the Merav/M6ConveyorAI protocol; it is not checked out by default and unrelated to the current app.
