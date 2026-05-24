# Tone Generator — Design

**Date:** 2026-05-24
**Status:** Approved

## Purpose

A small native Windows tone generator that produces pure sine tones from first principles — PCM samples are computed in-app and handed to the OS via `waveOut`. The UI uses the unmanifested Win32 control set so it renders in the classic gray-bevel style. Targets meditation / brainwave-entrainment use cases (Schumann, solfeggio, etc.) with a binaural mode for sub-audible beat frequencies.

## Constraints & Decisions

| Decision | Choice | Rationale |
|---|---|---|
| Language | C (C99) | Matches "incredibly efficient" criterion; pairs naturally with Win32 API which is a C API. |
| Compiler | MinGW-w64 `gcc` (already installed via scoop) | No MSVC available; MinGW ships full Win32 headers + `libwinmm`. |
| Build | Hand-written Makefile invoked by `mingw32-make` (fallback: `make`) | CMake is overkill for ~600 LOC, four `.c` files. |
| GUI framework | Pure Win32 (`user32`, `gdi32`, `comctl32`) — **no** Common Controls v6 manifest | "Classic look" emerges automatically without the v6 manifest; controls fall back to USER renderer. |
| Audio backend | `waveOut*` (Windows Multimedia API) | Simplest API that takes raw PCM. Modern alternatives (WASAPI) require COM and more boilerplate without benefit for this use case. |
| Sample format | 44.1 kHz, signed 16-bit, stereo, interleaved | Standard CD-quality; stereo is required for binaural mode. |
| Polyphony | One tone in mono mode OR two channels in binaural mode (toggleable) | Per user — no >2-voice mixer. |
| Waveform | Sine only | Per user. |
| Presets | Hardcoded table + user-typed custom field | Per user — no file I/O. |
| Click prevention | 10 ms linear fade in on play, 10 ms linear fade out on stop | Per user. |

## Architecture

```
Tone-Generator/
├── Makefile
├── docs/superpowers/specs/2026-05-24-tone-generator-design.md   (this file)
└── src/
    ├── main.c        — WinMain, owns AudioState, glues GUI ↔ audio
    ├── audio.h
    ├── audio.c       — waveOut lifecycle, sine synthesis, fade ramps, thread-safe param updates
    ├── gui.h
    ├── gui.c         — window creation, controls, WM_COMMAND dispatch
    ├── presets.h
    └── presets.c     — preset table + name→Hz lookup
```

Four files keep responsibilities visibly separated; each one is small enough to hold in mind at once. Headers expose narrow C interfaces (≈5–10 functions each).

### Module interfaces (sketch)

**`audio.h`**
```c
typedef struct AudioState AudioState;

typedef struct {
    double left_hz;     // 0 means silent on this channel
    double right_hz;    // equals left_hz in mono mode
    double volume;      // 0.0 .. 1.0
} AudioParams;

AudioState* audio_init(void);             // opens waveOut, allocates buffers, starts callback
void audio_shutdown(AudioState*);         // stops + closes device, frees buffers
void audio_set_params(AudioState*, AudioParams); // thread-safe, called from GUI thread
void audio_play(AudioState*);             // begins fade-in
void audio_stop(AudioState*);             // begins fade-out, then resets device
int  audio_is_playing(AudioState*);       // for status display
```

**`gui.h`**
```c
typedef struct GuiState GuiState;

GuiState* gui_create(HINSTANCE, AudioState*);  // creates main window, wires controls to audio
int       gui_run_message_loop(GuiState*);     // standard Win32 GetMessage/Dispatch loop
void      gui_destroy(GuiState*);
```

**`presets.h`**
```c
typedef struct { const char* name; double hz; } Preset;
const Preset* presets_list(int* out_count);    // returns pointer to static array
```

## Audio design (detailed)

### Sample generation
- Internal sample rate: **44 100 Hz**, 16-bit signed, stereo interleaved.
- Per-channel **phase accumulator** stored in `AudioState`:
  ```
  phase_l += 2π · left_hz  / SAMPLE_RATE   // wrap to [0, 2π)
  phase_r += 2π · right_hz / SAMPLE_RATE
  sample_l = (int16_t)(sin(phase_l) * volume * current_gain * 32767)
  sample_r = (int16_t)(sin(phase_r) * volume * current_gain * 32767)
  ```
- Phase accumulator (not `sin(2π·f·t)`) is mandatory: when the user changes frequency, the phase stays continuous and there is no audible click.

### Buffering
- 4 rotating `WAVEHDR` buffers, each 882 frames ≈ 20 ms.
- Open `waveOut` with `CALLBACK_FUNCTION`. On `WOM_DONE`, the callback refills the buffer and re-queues it with `waveOutWrite`. Callback runs on a system-managed thread.
- Callback computation is trivial (sin × constant per sample) so doing it inside the callback is acceptable — no separate worker thread needed.

### Thread-safe parameter updates
- A `CRITICAL_SECTION` guards the live `AudioParams` struct.
- GUI thread calls `audio_set_params()` → acquires the CS, copies the struct, releases.
- Audio callback acquires the CS *once at the top of each buffer fill*, copies into a local struct, releases, then computes the buffer. (Holding the lock for the whole fill would block the GUI; releasing first keeps GUI responsive.)

### Fade ramps
- `current_gain` lives in `AudioState`, modified only by the audio callback.
- `target_gain` is set by `audio_play()` (→ 1.0) or `audio_stop()` (→ 0.0).
- Each buffer, `current_gain` moves toward `target_gain` by a fixed step of `1.0 / (0.010 · SAMPLE_RATE)` per sample → reaches target in 10 ms.
- Once `current_gain == 0.0` and `target_gain == 0.0`, the callback signals an event; main thread (or a flag-checking helper) calls `waveOutReset` to flush queued buffers. (Calling `waveOutReset` while audio is still ramping would itself cause a click — we wait for silence first.)

## GUI design (detailed)

### Window
- Fixed-size dialog-style window, ~400×340 px, non-resizable (no `WS_THICKFRAME`).
- Title: "Tone Generator".
- **No manifest file embedded** → controls render in classic style on Windows 11.

### Layout
Three group boxes stacked vertically:

```
┌─ Frequency ──────────────────────────────┐
│  Preset: [Schumann 7.83 Hz       ▼]      │
│  Custom: [   432.000   ] Hz              │
└──────────────────────────────────────────┘
┌─ Mode ───────────────────────────────────┐
│  ( ) Single tone   (•) Binaural          │
│  Base: [ 100.000 ] Hz  Beat: [ 7.830 ] Hz│
│  [ ] Advanced (set L/R directly)         │
│  L:   [ 100.000 ] Hz   R:   [107.830] Hz │
└──────────────────────────────────────────┘
┌─ Output ─────────────────────────────────┐
│  Volume: ├──────●──────┤  50 %           │
│  [   Play   ]   [   Stop   ]             │
│  Status: Idle                            │
└──────────────────────────────────────────┘
```

### Control behavior
- **Preset combo:** writes selection's Hz into the *currently active primary field* — `Custom` in single mode, `Base` in binaural mode. Other fields are left as the user set them.
- **Custom edit:** active only in single mode. On focus loss or Enter, validates as a positive float; pushes to audio. Sets preset combo to "(custom)" if value doesn't match a preset within 0.001 Hz.
- **Mode radios:**
  - **Single** → Custom is active; entire Mode group (Base/Beat/Advanced/L/R) is greyed. Audio gets `left_hz = right_hz = custom_hz` (centered tone, both ears).
  - **Binaural** → Custom is greyed; Mode group becomes active (with Advanced controlling which subset, see below).
- **Advanced checkbox:**
  - Unchecked (default) → Base + Beat edits active, L + R greyed and auto-computed (`L = base`, `R = base + beat`).
  - Checked → L + R edits active, Base + Beat greyed.
- **Volume trackbar:** 0–100, updates audio in real time. Label to its right shows `nn %`.
- **Play/Stop:** Play calls `audio_play()` after pushing current params; Stop calls `audio_stop()`. Status label reflects `audio_is_playing()`.
- **Sub-audible warning:** if single-tone Hz < 20 or > 20 000, status label shows e.g. "Idle — note: 7.83 Hz is sub-audible; try Binaural mode." (Doesn't block — just informs.)

### Validation
- Edit fields reject non-numeric characters at WM_CHAR; on commit clamp to `[0.001, 22050.0]` (Nyquist for 44.1 kHz).
- Invalid input → revert to last good value.

## Preset table

Defined in `presets.c` as a static array:

| Name | Hz | Note |
|---|---|---|
| Schumann resonance | 7.83 | sub-audible; meant for binaural beat |
| Gamma / insight | 40.0 | edge of audibility |
| Middle C | 261.63 | reference |
| Concert A (Verdi 432) | 432.0 | |
| Concert A (standard) | 440.0 | reference |
| Solfeggio 174 | 174.0 | |
| Solfeggio 285 | 285.0 | |
| Solfeggio UT (396) | 396.0 | |
| Solfeggio RE (417) | 417.0 | |
| Solfeggio MI (528) | 528.0 | |
| Solfeggio FA (639) | 639.0 | |
| Solfeggio SOL (741) | 741.0 | |
| Solfeggio LA (852) | 852.0 | |
| Solfeggio 963 | 963.0 | |

User fills/edits this list themselves as part of the Learning Mode contribution.

## Build

`Makefile` (single rule):
```make
CC      = gcc
CFLAGS  = -O2 -Wall -Wextra -std=c99 -Isrc
LDFLAGS = -mwindows
LIBS    = -lwinmm -lgdi32 -luser32 -lcomctl32
SRC     = src/main.c src/audio.c src/gui.c src/presets.c
OUT     = ToneGen.exe

$(OUT): $(SRC)
	$(CC) $(CFLAGS) $(SRC) -o $(OUT) $(LDFLAGS) $(LIBS)

clean:
	-del /Q $(OUT)
```

`-mwindows` removes the console window. No manifest → classic look.

## Error handling

- `waveOutOpen` failure → `MessageBox` with error string from `waveOutGetErrorText`, then exit cleanly.
- `waveOutWrite` failure inside the callback → stop the device, set a flag, GUI status shows "Audio error".
- Out-of-memory at startup → `MessageBox` + exit; nothing to do after that.
- No silent failures — every error path either recovers or surfaces to the user.

## Testing

Manual smoke test checklist (no automated tests — tiny app, no logic worth mocking):

1. Builds cleanly with `mingw32-make`.
2. Window appears with classic chrome (no flat Win11 styling).
3. Selecting "Concert A (440)" + Play → hear a clean 440 Hz sine, no clicks.
4. Changing Custom field to 432 while playing → frequency shifts smoothly (no pop).
5. Switching to Binaural, Base=100, Beat=7.83 → headphones produce ~7.83 Hz wobble sensation.
6. Toggling Advanced → L/R edits become active, Base/Beat greyed.
7. Volume slider responds in real time.
8. Stop → fade out, no click. Play again → fade in, no click.
9. Closing window during playback → no crash, audio stops cleanly.

## Learning Mode contribution points

Two spots flagged for the user to write personally:

1. **`presets.c`** — the preset array. Scaffolded with the struct + first two entries; user adds the rest from the table above (or their own favorites).
2. **`audio.c::fill_buffer()`** — the inner sample loop. Scaffolded with the function signature, the param-copy boilerplate, the phase-accumulator setup, and the buffer-write housekeeping. User writes the ~8-line loop body that produces the actual PCM samples.

## Out of scope (deliberately)

- Multiple waveforms (square / triangle / saw / noise).
- More than two simultaneous tones / mixer UI.
- Saving custom presets to disk.
- VST plugin host / MIDI input.
- Audio recording / export to WAV.
- Cross-platform support — Windows only.
- A bundled installer; ship the `.exe` standalone.
