# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## What this is

A single-file Tkinter desktop app (`scale_sampler.py`) for sampling/testing a Swan weighing scale head over RS-232 Serial only. It supports a manual sampling mode, an automatic threshold-triggered weighing mode, a continuous LIVE mode, and a Swan calibration wizard (PC0035).

## Running

```bash
pip install pyserial
python scale_sampler.py
```

Or on Windows, double-click / run `run.bat` (installs `pyserial` if missing, then launches the app; pauses the console on error so the traceback stays visible).

There is no test suite, linter, or build step in this repo — verification is manual, by running the app against a real or simulated Swan scale head.

Settings (port, baud, mode, thresholds, etc.) persist to `scale_sampler_config.json` next to the script, loaded on startup and saved on connect. This file is gitignored — treat it as local machine state, not something to commit.

## Architecture

Everything lives in `scale_sampler.py`:

- **Module-level frame parsers** — pure functions returning `(weight: float|None, status_str: str, special: str|None)`:
  - `parse_swan_frame` — Swan text mode: `<+/-><7 chars, digits with an optional single '.'><CR>`. The decimal point's position (or absence) is not assumed fixed — if no `.` is present the value is treated as an integer scaled by `/1000`. This is what real Swan heads in the field actually send; do not tighten this back to a fixed-position regex.
  - `parse_swan_frame_bytes` — Swan PC0034 binary 20-byte frame (kept for reference/future use; no current call site uses it — all sampling threads use the text protocol, which is the format Swan heads actually send over the wire).
  - Calibration (PC0035) is a separate, ESC-based text command protocol handled entirely inside `_calib_thread`, unrelated to the weight-reading parsers above.

- **`ScaleSampler`** — the entire app: builds the UI (`_build_ui`), owns the RS-232 connection (`_connect`/`_disconnect` via `pyserial`), and runs one of four background daemon threads depending on the selected mode:
  - `_sample_thread` — one-shot: sample for N seconds, then summarize.
  - `_auto_thread` — threshold-triggered: watches live weight, starts a "weighing session" when it crosses `threshold`, ends it after `DEBOUNCE_S` below threshold, emits one summary per event.
  - `_live_thread` — repeating fixed-size windows (like `_sample_thread` but loops continuously, one summary per window).
  - `_calib_thread` — drives the Swan PC0035 calibration wizard dialog (`_open_swan_calib`) through its fixed step sequence (clear platform → zero → load weight → save/reboot), reading ESC-terminated text responses.

  **Threading contract**: background threads never touch Tk widgets or `tk.*Var`s directly. They read Tk-derived config values only once at thread start (passed in as args), then communicate exclusively by pushing `(kind, payload)` tuples onto `self.ui_queue`. `_pump_queue`, scheduled via `root.after(40, ...)`, is the only place queue messages are drained and applied to the UI — this is what keeps the Tk mainloop single-writer-safe. When adding a new async operation, follow this pattern rather than mutating widgets from a thread.

  `_decide_weight` is the shared algorithm for turning a burst of raw readings into one settled value: for ≥6 readings it slides a window of 6 and picks the flattest one (lowest stdev), returning its median — this rides through the ramp-up/ramp-down of a conveyor scale. Below 6 readings it falls back to a plain median.

## Protocol notes (see also the module docstring at the top of `scale_sampler.py`)

- **Swan weight reading**: send a bare `W` (no `\r\n`; a trailing `\r\n` gets interpreted as two separate commands and causes a double response) and read back `<+/-><7 chars><CR>`.
- **Swan calibration (PC0035)**: a distinct ESC-command protocol (`\x1B\x50`/`\x1B\x4E`/`\x1B\x65`, i.e. ESC P / ESC N / ESC e) used only by the calibration wizard, never for normal weight polling.
- Reference docs for the two Swan protocols are in `Swan/PC0034.docx` (weight/binary frame) and `Swan/PC0035.docx` (calibration).

## Repo layout notes

- `M6ConveyorAI` is a git submodule (see `.gitmodules`) left over from when this tool also supported the Merav/M6ConveyorAI protocol; it is not checked out by default and unrelated to the current Swan-only app.
