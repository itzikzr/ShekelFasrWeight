# Swan SC — Serial Communication Protocol

## Connection Parameters

| Parameter | Value |
|-----------|-------|
| Baud rate | 115200 (default) |
| Data bits | 8 |
| Parity | None |
| Stop bits | 1 |
| Flow control | None |
| Connector | RS-232 (USART1) |

---

## Weight Packet Format

Every weight response (from `W`, streaming `S`, zero/tare confirmation) uses this format:

```
[sign][d6][d5][d4][d3][d2][d1][d0][CR]
```

- **9 bytes total**: 1 sign + 7 digits + CR (0x0D)
- **sign**: `+` (positive) or `-` (negative)
- **digits**: right-justified, leading zeros, value in **grams** (integer)
- **decimal point position** is controlled by the `Q` (Disform) setting:

| Disform index | Format | Example (12345g) |
|---------------|--------|-----------------|
| 0 | `XXXXXXX` | `+0012345` |
| 1 | `XXXXX.X` | `+0001234.5` → not applicable at 1g resolution |
| 2 | `XXXX.XX` | shown as `+XXXX.XX` |
| 3 | `XXX.XXX` | `+012.345` |
| 4 | `XX.XXXX` | `+01.2345` |
| 5 | `X.XXXXX` | `+0.12345` |

> The decimal point is visual only — the underlying value is always in grams multiplied by the decimal factor.

---

## Direct Commands (single character, no ESC)

Send one ASCII character. The device responds immediately (or asynchronously for Z and T).

### W — Weight query

| Direction | Data |
|-----------|------|
| Send | `W` |
| Receive | `[+/-][XXXXXXX]\r` |

Returns current net weight in the format described above.

---

### S — Start streaming

| Direction | Data |
|-----------|------|
| Send | `S` |
| Receive (immediate) | `\rStream ON\n` |
| Receive (continuous) | `[+/-][XXXXXXX]\r` every ~109 ms |

After sending `S`, the device sends a weight packet automatically on every sample (~366 Hz internal, decimated to ~9 packets/sec). Stream packets go to the port that sent `S`.

---

### s — Stop streaming

| Direction | Data |
|-----------|------|
| Send | `s` (lowercase) |
| Receive | `\rStream OFF\n` |

> **Note:** must be lowercase `s`. Uppercase `S` starts streaming.

---

### Z — Zero (tare to zero)

| Direction | Data |
|-----------|------|
| Send | `Z` |
| Receive (async, ~500 ms) | `Z\r` = success |
| Receive (async, ~500 ms) | `F\r` = failed |

The device initiates a zero operation. The response arrives asynchronously after the operation completes:
- `Z\r` — zero was accepted and applied (scale was within D/4 of zero)
- `F\r` — zero was rejected (overload, scale locked, or out of range)

---

### T — Tare

| Direction | Data |
|-----------|------|
| Send | `T` |
| Receive (immediate) | `F\r` = rejected immediately (scale locked, overload, or busy) |
| Receive (async, 1–4 sec) | `T\r` = tare applied successfully |
| Receive (async, 1–4 sec) | `F\r` = tare timeout (scale not stable within 3 sec) |

The device waits 1 second, then waits up to 3 seconds for the scale to stabilize:
- `T\r` — tare was applied
- `F\r` — tare failed (not stable, or rejected immediately)

---

### C — Clear tare

| Direction | Data |
|-----------|------|
| Send | `C` |
| Receive | *(no response)* |

Clears the active tare and returns to gross weight display. Only effective when tare is active (`dnet=1`).

---

### F — Full scale query / set

**Query:**

| Direction | Data |
|-----------|------|
| Send | `F` |
| Receive | `\rFull=XXXXXXXg\n` |

Shows the current full-scale value in grams.

**Set (send digits then CR):**

| Direction | Data |
|-----------|------|
| Send | `F` |
| Receive | `\rFull=XXXXXXXg\n` |
| Send | `<digits><CR>` (up to 8 digits) |
| Receive | `\rFull=XXXXXXXg saved\n` |

After receiving the `F` response, the device waits silently for optional digits followed by CR:
- Digits + `CR` → sets new Full value, saves to NVRAM
- Any other character → cancels silently (also processes that character as a new command)

**Example:** to set Full = 30000g:
```
→ F
← \rFull=0099999g\n
→ 30000\r
← \rFull=0030000g saved\n
```

---

### B — Baud rate query / set

**Query:**

| Direction | Data |
|-----------|------|
| Send | `B` |
| Receive | `\rBaud=XXXXXXX\r0=4800\r1=9600\r2=14400\r3=19200\r4=28800\r5=38400\r6=57600\r7=115200\n` |

**Set:**

| Direction | Data |
|-----------|------|
| Send | `B` |
| Receive | *(menu as above)* |
| Send | `<digit 0–7>` |
| Receive | `\rSaved\n` then device reboots at new baud rate |

> After setting, the device reboots. Reconnect at the new baud rate.

| Index | Baud rate |
|-------|-----------|
| 0 | 4800 |
| 1 | 9600 |
| 2 | 14400 |
| 3 | 19200 |
| 4 | 28800 |
| 5 | 38400 |
| 6 | 57600 |
| **7** | **115200** ← default |

---

### R — Rounding step query / set

**Query:**

| Direction | Data |
|-----------|------|
| Send | `R` |
| Receive | `\rRound=X\r0=1g\r1=1g\r2=2g\r3=5g\r4=10g\r5=20g\r6=50g\r7=100g\n` |

**Set:**

| Direction | Data |
|-----------|------|
| Send | `R` |
| Receive | *(menu as above)* |
| Send | `<digit 0–7>` |
| Receive | `\rSaved\n` |

| Index | Step (kg mode) |
|-------|---------------|
| 0 | 1g |
| 1 | 1g |
| 2 | 2g |
| **3** | **5g** ← default |
| 4 | 10g |
| 5 | 20g |
| 6 | 50g |
| 7 | 100g |

---

### Q — Decimal format (Disform) query / set

**Query:**

| Direction | Data |
|-----------|------|
| Send | `Q` |
| Receive | `\rDisform=X\r0=XXXXXXX\r1=XXXXX.X\r2=XXXX.XX\r3=XXX.XXX\r4=XX.XXXX\r5=X.XXXXX\n` |

**Set:**

| Direction | Data |
|-----------|------|
| Send | `Q` |
| Receive | *(menu as above)* |
| Send | `<digit 0–5>` |
| Receive | `\rSaved\n` |

Controls the decimal point position in the weight packet.

---

### I — Info

| Direction | Data |
|-----------|------|
| Send | `I` |
| Receive | *(device info string)* |

---

### A — Raw AD value

| Direction | Data |
|-----------|------|
| Send | `A` |
| Receive | `\rXXXXXXXXXX\n` |

Returns the raw analog-to-digital converter value (brut), no processing applied. Useful for calibration diagnostics.

---

### V — Version

| Direction | Data |
|-----------|------|
| Send | `V` |
| Receive | `\rID XXXXX.XXXX.XXXX\n` |

Returns firmware version string.

---

## ESC Protocol Commands

Format: `ESC` (0x1B) + command letter + optional value bytes + `ESC` + `E`

These commands are for factory/service use only.

| Command | Sequence | Action |
|---------|----------|--------|
| Calibration | `ESC P <load_kg_as_float> ESC E` | Set calibration load and start calibration |
| Factory reset | `ESC D ESC E` | Reset all NVRAM to factory defaults + reboot |
| Next step | `ESC N ESC E` | Advance calibration/program state machine |

---

## Response Summary Table

| Send | Receive | Timing |
|------|---------|--------|
| `W` | `[+/-][XXXXXXX]\r` | Immediate |
| `S` | `\rStream ON\n` then weight packets | Immediate + continuous |
| `s` | `\rStream OFF\n` | Immediate |
| `Z` | `Z\r` or `F\r` | ~500 ms async |
| `T` | `T\r` or `F\r` | 1–4 sec async |
| `C` | *(none)* | — |
| `F` | `\rFull=XXXXXXXg\n` | Immediate |
| `F` + digits + CR | `\rFull=XXXXXXXg saved\n` | Immediate |
| `B` | baud menu | Immediate |
| `B` + `<0–7>` | `\rSaved\n` + reboot | Immediate then reboot |
| `R` | round menu | Immediate |
| `R` + `<0–7>` | `\rSaved\n` | Immediate |
| `Q` | disform menu | Immediate |
| `Q` + `<0–5>` | `\rSaved\n` | Immediate |
| `I` | info string | Immediate |
| `A` | `\rXXXXXXXXXX\n` | Immediate |
| `V` | `\rID ...\n` | Immediate |

---

## Notes for Software Implementation

1. **Streaming mode** is the recommended approach for a conveyor application — send `S` once and read weight packets continuously. Send `s` to stop.

2. **Zero and Tare** return async responses. After sending `Z` or `T`, wait up to 5 seconds for `Z\r`/`T\r` (success) or `F\r` (failure) before timing out.

3. **Two-step commands** (B, R, Q, F): after sending the first character and receiving the menu, send the digit immediately. CR and LF between the first char and the digit are ignored by the device. Any non-digit character (other than CR/LF) cancels the menu and is processed as a new command.

4. **Weight value**: the packet always contains 7 digits in grams. Apply the decimal point according to the `Q` (Disform) setting. For example, with Disform=3 (`XXX.XXX`), the value `+0012345` means 12.345 kg.

5. **Default NVRAM settings**:
   - Baud: 115200
   - FilterIndex: 0 (fastest, ~366 Hz)
   - Oscillation max: 0 (instant stability)
   - Rounding: index 3 (5g steps)
   - Full scale: 99999g

6. **F\r ambiguity**: `F\r` is the failure response for both `Z` and `T`. Track which command is pending to know which failed.
