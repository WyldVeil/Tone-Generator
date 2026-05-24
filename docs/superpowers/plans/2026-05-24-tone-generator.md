# Tone Generator Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Build a native Win32 tone generator in C99 that synthesises pure sine tones from first principles (PCM samples computed in-app, played via `waveOut`), with a classic gray-bevel GUI, optional binaural mode, hardcoded preset table, custom-frequency entry, and click-free fade ramps.

**Architecture:** Single-process Win32 app. GUI thread owns the window + controls; `waveOut` callback runs on a system-managed thread. A `CRITICAL_SECTION`-guarded `AudioParams` struct passes live parameter changes from GUI → audio with zero blocking. Pure synthesis functions (`synth_fill_buffer`, `synth_next_gain`) are separated from the `waveOut` plumbing so they can be unit-tested without opening a sound device.

**Tech Stack:** C99 · MinGW-w64 gcc · Win32 API (`user32`, `gdi32`, `comctl32`) · Windows Multimedia API (`winmm` → `waveOut*`) · `make` for build · per-file C test programs with `assert.h` for the testable parts.

**Spec:** `docs/superpowers/specs/2026-05-24-tone-generator-design.md`

**Learning Mode:** Disabled — implementer writes all code, including former LEARNING SPOTs at Task 3 Step 4 and Task 5 Step 4.

---

## File map

```
Tone-Generator/
├── Makefile                       (new — Task 1)
├── src/
│   ├── main.c                     (new — Task 9)
│   ├── audio.h / audio.c          (new — Tasks 4 + 6)
│   ├── synth.h / synth.c          (new — Tasks 2 + 5; pure functions only, no Windows deps)
│   ├── gui.h   / gui.c            (new — Tasks 7 + 8)
│   └── presets.h / presets.c      (new — Task 3)
└── tests/
    ├── test_synth.c               (new — Tasks 2 + 5)
    └── test_presets.c             (new — Task 3)
```

**Note on file layout:** the spec called for synthesis code to live in `audio.c`. During planning we split the pure synthesis math into `synth.c` / `synth.h` so it can be unit-tested without depending on Windows headers or opening a sound device. `audio.c` still owns the `waveOut` lifecycle; it calls into `synth_*` for the math. This is a small refinement that serves testability without changing any external behavior.

---

### Task 1: Project scaffolding & build verification

**Goal:** Create the directory layout, write a `Makefile`, drop in empty stubs, and verify that the empty project compiles and links cleanly. This catches MinGW / linker / path issues before any real code is written.

**Files:**
- Create: `Makefile`
- Create: `src/main.c` (stub `WinMain` returning 0)
- Create: `src/audio.h`, `src/audio.c` (empty)
- Create: `src/synth.h`, `src/synth.c` (empty)
- Create: `src/gui.h`, `src/gui.c` (empty)
- Create: `src/presets.h`, `src/presets.c` (empty)

- [ ] **Step 1: Create the `Makefile`**

```make
CC      := gcc
CFLAGS  := -O2 -Wall -Wextra -std=c99 -Isrc
LDFLAGS := -mwindows
LIBS    := -lwinmm -lgdi32 -luser32 -lcomctl32

SRC := src/main.c src/audio.c src/synth.c src/gui.c src/presets.c
OUT := ToneGen.exe

TEST_LDFLAGS :=
TEST_LIBS    := -lwinmm
TESTS := test_synth.exe test_presets.exe

$(OUT): $(SRC)
	$(CC) $(CFLAGS) $(SRC) -o $(OUT) $(LDFLAGS) $(LIBS)

test_synth.exe: tests/test_synth.c src/synth.c src/synth.h
	$(CC) -O0 -g -Wall -Wextra -std=c99 -Isrc tests/test_synth.c src/synth.c -o test_synth.exe -lm

test_presets.exe: tests/test_presets.c src/presets.c src/presets.h
	$(CC) -O0 -g -Wall -Wextra -std=c99 -Isrc tests/test_presets.c src/presets.c -o test_presets.exe

test: $(TESTS)
	./test_synth.exe
	./test_presets.exe

clean:
	-del /Q $(OUT) test_synth.exe test_presets.exe 2>nul

.PHONY: test clean
```

- [ ] **Step 2: Create the empty source stubs**

For each of `src/audio.h`, `src/audio.c`, `src/synth.h`, `src/synth.c`, `src/gui.h`, `src/gui.c`, `src/presets.h`, `src/presets.c`, write a single-line header guard for the `.h` files and an empty `.c` body:

`src/audio.h`:
```c
#ifndef AUDIO_H
#define AUDIO_H
#endif
```

`src/audio.c`:
```c
#include "audio.h"
```

Repeat the same pattern for `synth`, `gui`, `presets` (each `.h` gets a unique guard name like `SYNTH_H`, `GUI_H`, `PRESETS_H`).

- [ ] **Step 3: Create `src/main.c`**

```c
#include <windows.h>

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrev, LPSTR cmd, int show) {
    (void)hInstance; (void)hPrev; (void)cmd; (void)show;
    return 0;
}
```

- [ ] **Step 4: Build it**

Run: `make`
Expected: produces `ToneGen.exe` in project root, no warnings, no errors.

If `make` is not on PATH but `mingw32-make` is, use that instead. (Earlier environment check confirmed `make` is available at `C:\Users\win11\scoop\shims\make.exe`.)

- [ ] **Step 5: Smoke-test the executable**

Run: `./ToneGen.exe`
Expected: process exits immediately with code 0, no console window appears (because of `-mwindows`).

- [ ] **Step 6: Commit**

```
git init
git add Makefile src/ .gitignore
git commit -m "scaffold: empty Win32 project skeleton with Makefile"
```

If no `.gitignore` exists, also create one containing:
```
ToneGen.exe
test_*.exe
*.o
*.obj
```

---

### Task 2: Pure-synth module — fade ramp (TDD)

**Goal:** Implement `synth_next_gain()`, the per-sample fade-ramp helper. This is the simplest pure function in the project and a good place to establish the test pattern.

**Files:**
- Modify: `src/synth.h`
- Modify: `src/synth.c`
- Create: `tests/test_synth.c`

- [ ] **Step 1: Declare the function in `src/synth.h`**

Replace the contents of `src/synth.h` with:
```c
#ifndef SYNTH_H
#define SYNTH_H

#include <stdint.h>
#include <stddef.h>

/* Move current toward target by at most step. Never overshoot.
 * Returns the new gain value. Step must be positive. */
double synth_next_gain(double current, double target, double step);

#endif
```

- [ ] **Step 2: Write the failing test**

Create `tests/test_synth.c`:
```c
#include "synth.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>

static int approx(double a, double b) { return fabs(a - b) < 1e-9; }

static void test_next_gain_moves_up_toward_target(void) {
    assert(approx(synth_next_gain(0.0, 1.0, 0.25), 0.25));
}

static void test_next_gain_moves_down_toward_target(void) {
    assert(approx(synth_next_gain(1.0, 0.0, 0.25), 0.75));
}

static void test_next_gain_does_not_overshoot_going_up(void) {
    assert(approx(synth_next_gain(0.9, 1.0, 0.25), 1.0));
}

static void test_next_gain_does_not_overshoot_going_down(void) {
    assert(approx(synth_next_gain(0.1, 0.0, 0.25), 0.0));
}

static void test_next_gain_at_target_returns_target(void) {
    assert(approx(synth_next_gain(0.5, 0.5, 0.1), 0.5));
}

int main(void) {
    test_next_gain_moves_up_toward_target();
    test_next_gain_moves_down_toward_target();
    test_next_gain_does_not_overshoot_going_up();
    test_next_gain_does_not_overshoot_going_down();
    test_next_gain_at_target_returns_target();
    printf("test_synth (fade): 5 passed\n");
    return 0;
}
```

- [ ] **Step 3: Run the test, watch it fail to build**

Run: `make test_synth.exe`
Expected: linker error — `undefined reference to synth_next_gain`.

- [ ] **Step 4: Implement the function**

Replace the contents of `src/synth.c` with:
```c
#include "synth.h"

double synth_next_gain(double current, double target, double step) {
    if (current < target) {
        double next = current + step;
        return next > target ? target : next;
    }
    if (current > target) {
        double next = current - step;
        return next < target ? target : next;
    }
    return current;
}
```

- [ ] **Step 5: Run the test, watch it pass**

Run: `make test`
Expected output includes: `test_synth (fade): 5 passed`

(The `test_presets.exe` target will fail to build because `presets.c` is still empty — that's fine for this task. If `make test` fails before reaching `test_synth`, run `make test_synth.exe && ./test_synth.exe` directly.)

- [ ] **Step 6: Commit**

```
git add src/synth.h src/synth.c tests/test_synth.c
git commit -m "synth: add fade-ramp helper with tests"
```

---

### Task 3: Presets module

**Goal:** Define the preset table and a lookup function with all 14 presets from the spec.

**Files:**
- Modify: `src/presets.h`
- Modify: `src/presets.c`
- Create: `tests/test_presets.c`

- [ ] **Step 1: Declare the interface in `src/presets.h`**

Replace the contents with:
```c
#ifndef PRESETS_H
#define PRESETS_H

typedef struct {
    const char* name;
    double      hz;
} Preset;

/* Returns a pointer to a static array of presets.
 * Writes the count to *out_count. Never returns NULL. */
const Preset* presets_list(int* out_count);

#endif
```

- [ ] **Step 2: Write the failing test**

Create `tests/test_presets.c`:
```c
#include "presets.h"
#include <assert.h>
#include <string.h>
#include <stdio.h>

static const Preset* find(const char* name) {
    int count = 0;
    const Preset* list = presets_list(&count);
    for (int i = 0; i < count; i++) {
        if (strcmp(list[i].name, name) == 0) return &list[i];
    }
    return 0;
}

int main(void) {
    int count = 0;
    const Preset* list = presets_list(&count);
    assert(list != 0);
    assert(count >= 3);   /* at least the seed entries */

    const Preset* schumann = find("Schumann resonance");
    assert(schumann != 0);
    assert(schumann->hz == 7.83);

    const Preset* a440 = find("Concert A (standard)");
    assert(a440 != 0);
    assert(a440->hz == 440.0);

    const Preset* solf528 = find("Solfeggio MI (528)");
    assert(solf528 != 0);
    assert(solf528->hz == 528.0);

    printf("test_presets: %d presets, 3 named lookups passed\n", count);
    return 0;
}
```

- [ ] **Step 3: Write `src/presets.c` with the full preset table**

```c
#include "presets.h"
#include <stddef.h>

static const Preset PRESETS[] = {
    { "Schumann resonance",        7.83 },
    { "Gamma / insight (40 Hz)",  40.00 },
    { "Middle C",                261.63 },
    { "Concert A (Verdi 432)",   432.00 },
    { "Concert A (standard)",    440.00 },
    { "Solfeggio 174",           174.00 },
    { "Solfeggio 285",           285.00 },
    { "Solfeggio UT (396)",      396.00 },
    { "Solfeggio RE (417)",      417.00 },
    { "Solfeggio MI (528)",      528.00 },
    { "Solfeggio FA (639)",      639.00 },
    { "Solfeggio SOL (741)",     741.00 },
    { "Solfeggio LA (852)",      852.00 },
    { "Solfeggio 963",           963.00 },
};

const Preset* presets_list(int* out_count) {
    *out_count = (int)(sizeof PRESETS / sizeof PRESETS[0]);
    return PRESETS;
}
```

- [ ] **Step 4: Run the test**

Run: `make test_presets.exe && ./test_presets.exe`
Expected: `test_presets: 14 presets, 3 named lookups passed`

- [ ] **Step 5: Commit**

```
git add src/presets.h src/presets.c tests/test_presets.c
git commit -m "presets: add preset table with lookup helper"
```

---

### Task 4: Audio module — interface & state types

**Goal:** Declare the public interface for the audio module — types and function signatures only. No implementation yet. This lets later tasks (GUI, main) reference the types without circular dependencies.

**Files:**
- Modify: `src/audio.h`

- [ ] **Step 1: Write the header**

Replace the contents of `src/audio.h` with:
```c
#ifndef AUDIO_H
#define AUDIO_H

typedef struct AudioState AudioState;

typedef struct {
    double left_hz;   /* 0 means silent on the left channel */
    double right_hz;  /* equals left_hz in mono mode */
    double volume;    /* 0.0 .. 1.0 */
} AudioParams;

/* Opens waveOut, allocates buffers, starts the callback. Returns NULL on failure. */
AudioState* audio_init(void);

/* Stops device, frees buffers. Safe to call with NULL. */
void audio_shutdown(AudioState* st);

/* Thread-safe. Pushes new live parameters; takes effect on next buffer boundary. */
void audio_set_params(AudioState* st, AudioParams params);

/* Begins fade-in if not already playing. */
void audio_play(AudioState* st);

/* Begins fade-out; when silent, resets the device. */
void audio_stop(AudioState* st);

/* Returns 1 if currently producing sound (including during fade ramps), 0 otherwise. */
int  audio_is_playing(AudioState* st);

#endif
```

- [ ] **Step 2: Verify it still compiles**

Run: `make`
Expected: builds successfully (the empty `audio.c` is fine — no implementations are required to compile, only when something calls them).

- [ ] **Step 3: Commit**

```
git add src/audio.h
git commit -m "audio: declare public interface and AudioParams type"
```

---

### Task 5: Pure-synth module — sample buffer fill

**Goal:** Implement `synth_fill_buffer()`, the function that turns `AudioParams` + current gain into 16-bit stereo PCM samples. This is the heart of the tone generator.

**Files:**
- Modify: `src/synth.h`
- Modify: `src/synth.c`
- Modify: `tests/test_synth.c`

- [ ] **Step 1: Extend `src/synth.h`**

Add the following declarations + constants below the existing `synth_next_gain` declaration, before the `#endif`:

```c
#define SYNTH_SAMPLE_RATE 44100

/* Per-channel phase accumulator. Phases are in radians [0, 2π).
 * Hold these in the caller across calls so that frequency changes
 * don't cause phase discontinuities (clicks). */
typedef struct {
    double phase_l;
    double phase_r;
} SynthPhase;

/* Parameters needed to render one buffer of audio.
 * `gain_start` is multiplied with each sample at frame 0 of the buffer,
 * then advances toward `gain_target` by `gain_step` per sample (per
 * synth_next_gain semantics). */
typedef struct {
    double left_hz;
    double right_hz;
    double volume;
    double gain_start;
    double gain_target;
    double gain_step;
} SynthFrameParams;

/* Fills `out` with `frames` stereo (L,R) interleaved int16 samples.
 * `out` must point to space for at least 2*frames int16_t values.
 * Updates `*phase` in place. Returns the final gain value reached. */
double synth_fill_buffer(int16_t* out, size_t frames,
                         SynthPhase* phase,
                         SynthFrameParams p);
```

- [ ] **Step 2: Add failing tests**

Append the following to `tests/test_synth.c` *before* `int main(void)`:

```c
static void test_fill_buffer_zero_freq_is_silent(void) {
    int16_t buf[200] = {0};
    SynthPhase phase = {0, 0};
    SynthFrameParams p = { 0.0, 0.0, 1.0, 1.0, 1.0, 0.0 };
    synth_fill_buffer(buf, 100, &phase, p);
    for (int i = 0; i < 200; i++) assert(buf[i] == 0);
}

static void test_fill_buffer_440hz_produces_nonzero(void) {
    int16_t buf[200] = {0};
    SynthPhase phase = {0, 0};
    SynthFrameParams p = { 440.0, 440.0, 1.0, 1.0, 1.0, 0.0 };
    synth_fill_buffer(buf, 100, &phase, p);
    int nonzero = 0;
    for (int i = 0; i < 200; i++) if (buf[i] != 0) nonzero++;
    assert(nonzero > 100);  /* most samples should be nonzero */
}

static void test_fill_buffer_advances_phase(void) {
    int16_t buf[200];
    SynthPhase phase = {0, 0};
    SynthFrameParams p = { 1000.0, 1000.0, 1.0, 1.0, 1.0, 0.0 };
    synth_fill_buffer(buf, 100, &phase, p);
    /* After 100 frames at 1000 Hz / 44100 Hz: phase_l should equal
     * (2π · 1000 / 44100) · 100, wrapped to [0, 2π). */
    double expected = (2.0 * 3.14159265358979323846 * 1000.0 / 44100.0) * 100.0;
    while (expected >= 2.0 * 3.14159265358979323846) expected -= 2.0 * 3.14159265358979323846;
    assert(fabs(phase.phase_l - expected) < 1e-6);
}

static void test_fill_buffer_applies_volume(void) {
    int16_t loud[200], quiet[200];
    SynthPhase pl = {0, 0}, pq = {0, 0};
    SynthFrameParams pa_loud  = { 440.0, 440.0, 1.0, 1.0, 1.0, 0.0 };
    SynthFrameParams pa_quiet = { 440.0, 440.0, 0.1, 1.0, 1.0, 0.0 };
    synth_fill_buffer(loud,  100, &pl, pa_loud);
    synth_fill_buffer(quiet, 100, &pq, pa_quiet);
    /* peak of quiet buffer should be roughly 1/10 the peak of loud buffer */
    int peak_loud = 0, peak_quiet = 0;
    for (int i = 0; i < 200; i++) {
        if (abs(loud[i])  > peak_loud)  peak_loud  = abs(loud[i]);
        if (abs(quiet[i]) > peak_quiet) peak_quiet = abs(quiet[i]);
    }
    assert(peak_quiet > 0);
    assert(peak_loud > peak_quiet * 5);  /* loose bound */
}

static void test_fill_buffer_ramps_gain(void) {
    int16_t buf[200];
    SynthPhase phase = {0, 0};
    /* Fade from gain 0 → 1 over 100 frames, step = 0.01 */
    SynthFrameParams p = { 440.0, 440.0, 1.0, 0.0, 1.0, 0.01 };
    double final_gain = synth_fill_buffer(buf, 100, &phase, p);
    assert(fabs(final_gain - 1.0) < 1e-9);
    /* First sample should be ≈ 0 (gain ≈ 0), later samples larger */
    assert(abs(buf[0]) < abs(buf[198]));
}
```

Then update `main` to call them:
```c
int main(void) {
    test_next_gain_moves_up_toward_target();
    test_next_gain_moves_down_toward_target();
    test_next_gain_does_not_overshoot_going_up();
    test_next_gain_does_not_overshoot_going_down();
    test_next_gain_at_target_returns_target();
    test_fill_buffer_zero_freq_is_silent();
    test_fill_buffer_440hz_produces_nonzero();
    test_fill_buffer_advances_phase();
    test_fill_buffer_applies_volume();
    test_fill_buffer_ramps_gain();
    printf("test_synth: 10 passed\n");
    return 0;
}
```

Also add `#include <stdlib.h>` near the top for `abs()`.

- [ ] **Step 3: Run tests, watch them fail**

Run: `make test_synth.exe`
Expected: linker error — `undefined reference to synth_fill_buffer`.

- [ ] **Step 4: Implement `synth_fill_buffer`**

Append to `src/synth.c`:

```c
#include <math.h>

#define TWO_PI 6.28318530717958647692

double synth_fill_buffer(int16_t* out, size_t frames,
                         SynthPhase* phase,
                         SynthFrameParams p)
{
    /* Per-sample phase increments (radians per sample).
     * If hz is 0, the channel produces silence regardless of phase. */
    double inc_l = TWO_PI * p.left_hz  / (double)SYNTH_SAMPLE_RATE;
    double inc_r = TWO_PI * p.right_hz / (double)SYNTH_SAMPLE_RATE;

    double pl = phase->phase_l;
    double pr = phase->phase_r;
    double gain = p.gain_start;

    for (size_t i = 0; i < frames; i++) {
        double sl = sin(pl) * p.volume * gain;
        double sr = sin(pr) * p.volume * gain;
        out[2*i]     = (int16_t)(sl * 32767.0);
        out[2*i + 1] = (int16_t)(sr * 32767.0);

        pl += inc_l;
        pr += inc_r;
        while (pl >= TWO_PI) pl -= TWO_PI;
        while (pr >= TWO_PI) pr -= TWO_PI;

        gain = synth_next_gain(gain, p.gain_target, p.gain_step);
    }

    phase->phase_l = pl;
    phase->phase_r = pr;
    return gain;
}
```

Notes on the algorithm:
- The phase accumulator (`pl`, `pr`) is the reason changing frequency mid-tone doesn't click — phase stays continuous.
- Gain and volume are both applied before the int16 quantization, so the fade ramp stays smooth even at low volumes.
- Wrapping `pl`/`pr` back into `[0, 2π)` prevents floating-point precision degradation over long runs.

- [ ] **Step 5: Run the tests**

Run: `make test`
Expected output:
```
test_synth: 10 passed
test_presets: N presets, 3 named lookups passed
```

If a test fails, the assertion message will point at the line. Common bugs:
- Forgetting to multiply by `32767.0` → all samples are 0 or ±1.
- Forgetting to advance `pl`/`pr` → constant DC offset, no oscillation.
- Forgetting to advance `gain` → ramp test fails.
- Casting `sin(...)` directly to `int16_t` before multiplying by `32767` → silence.

- [ ] **Step 6: Commit**

```
git add src/synth.h src/synth.c tests/test_synth.c
git commit -m "synth: add fill_buffer with phase accumulator and gain ramp"
```

---

### Task 6: Audio module — waveOut implementation

**Goal:** Implement the `AudioState` struct and all six `audio_*` functions on top of `waveOut*`. This is the bridge between the pure synth math and the operating system's audio device.

**Files:**
- Modify: `src/audio.c`

- [ ] **Step 1: Write the full audio.c**

Replace the contents of `src/audio.c` with:

```c
#include "audio.h"
#include "synth.h"

#include <windows.h>
#include <mmsystem.h>
#include <stdlib.h>
#include <string.h>

#define BUFFER_FRAMES   882                /* ~20 ms at 44.1 kHz */
#define BUFFER_COUNT    4
#define FADE_MS         10
#define FADE_STEP       (1.0 / (FADE_MS * 0.001 * SYNTH_SAMPLE_RATE))

struct AudioState {
    HWAVEOUT        hwo;
    WAVEHDR         hdr[BUFFER_COUNT];
    int16_t*        buf[BUFFER_COUNT];
    CRITICAL_SECTION cs;
    AudioParams     params;          /* live, guarded by cs */
    SynthPhase      phase;           /* owned by callback thread */
    double          current_gain;    /* owned by callback thread */
    double          target_gain;     /* guarded by cs */
    int             playing;         /* 1 between play() and audio going silent */
    int             shutting_down;   /* signal from main → callback */
};

static void CALLBACK wave_callback(HWAVEOUT hwo, UINT msg,
                                   DWORD_PTR inst, DWORD_PTR p1, DWORD_PTR p2)
{
    (void)hwo; (void)p1; (void)p2;
    if (msg != WOM_DONE) return;
    AudioState* st = (AudioState*)inst;
    WAVEHDR* hdr = (WAVEHDR*)p1;

    if (st->shutting_down) return;

    /* Snapshot live params + target_gain under lock, then release before
     * doing the (relatively slow) sample math so the GUI thread never
     * waits on us. */
    AudioParams snap;
    double      target;
    EnterCriticalSection(&st->cs);
    snap   = st->params;
    target = st->target_gain;
    LeaveCriticalSection(&st->cs);

    SynthFrameParams fp = {
        .left_hz     = snap.left_hz,
        .right_hz    = snap.right_hz,
        .volume      = snap.volume,
        .gain_start  = st->current_gain,
        .gain_target = target,
        .gain_step   = FADE_STEP,
    };

    st->current_gain = synth_fill_buffer((int16_t*)hdr->lpData,
                                         BUFFER_FRAMES, &st->phase, fp);

    /* If we've faded to silence and were stopping, mark not-playing. */
    if (st->current_gain == 0.0 && target == 0.0) {
        st->playing = 0;
    }

    waveOutWrite(hwo, hdr, sizeof *hdr);
}

AudioState* audio_init(void) {
    AudioState* st = (AudioState*)calloc(1, sizeof *st);
    if (!st) return NULL;
    InitializeCriticalSection(&st->cs);
    st->params.volume = 0.5;

    WAVEFORMATEX fmt = {
        .wFormatTag      = WAVE_FORMAT_PCM,
        .nChannels       = 2,
        .nSamplesPerSec  = SYNTH_SAMPLE_RATE,
        .wBitsPerSample  = 16,
        .nBlockAlign     = 4,                           /* 2 ch * 16 bit / 8 */
        .nAvgBytesPerSec = SYNTH_SAMPLE_RATE * 4,
        .cbSize          = 0,
    };

    MMRESULT r = waveOutOpen(&st->hwo, WAVE_MAPPER, &fmt,
                             (DWORD_PTR)wave_callback,
                             (DWORD_PTR)st, CALLBACK_FUNCTION);
    if (r != MMSYSERR_NOERROR) {
        char msg[256];
        waveOutGetErrorTextA(r, msg, sizeof msg);
        MessageBoxA(NULL, msg, "waveOutOpen failed", MB_ICONERROR);
        DeleteCriticalSection(&st->cs);
        free(st);
        return NULL;
    }

    /* Allocate buffers, prime them with silence, queue them. */
    for (int i = 0; i < BUFFER_COUNT; i++) {
        st->buf[i] = (int16_t*)calloc(BUFFER_FRAMES * 2, sizeof(int16_t));
        st->hdr[i].lpData         = (LPSTR)st->buf[i];
        st->hdr[i].dwBufferLength = BUFFER_FRAMES * 2 * sizeof(int16_t);
        waveOutPrepareHeader(st->hwo, &st->hdr[i], sizeof st->hdr[i]);
        waveOutWrite(st->hwo, &st->hdr[i], sizeof st->hdr[i]);
    }
    return st;
}

void audio_shutdown(AudioState* st) {
    if (!st) return;
    st->shutting_down = 1;
    waveOutReset(st->hwo);
    for (int i = 0; i < BUFFER_COUNT; i++) {
        waveOutUnprepareHeader(st->hwo, &st->hdr[i], sizeof st->hdr[i]);
        free(st->buf[i]);
    }
    waveOutClose(st->hwo);
    DeleteCriticalSection(&st->cs);
    free(st);
}

void audio_set_params(AudioState* st, AudioParams params) {
    if (!st) return;
    EnterCriticalSection(&st->cs);
    st->params = params;
    LeaveCriticalSection(&st->cs);
}

void audio_play(AudioState* st) {
    if (!st) return;
    EnterCriticalSection(&st->cs);
    st->target_gain = 1.0;
    st->playing     = 1;
    LeaveCriticalSection(&st->cs);
}

void audio_stop(AudioState* st) {
    if (!st) return;
    EnterCriticalSection(&st->cs);
    st->target_gain = 0.0;
    LeaveCriticalSection(&st->cs);
    /* Don't reset the device here — let the callback finish the fade-out.
     * When current_gain reaches 0, callback clears st->playing. */
}

int audio_is_playing(const AudioState* st) {
    if (!st) return 0;
    return st->playing;
}
```

- [ ] **Step 2: Build**

Run: `make`
Expected: clean build, no warnings.

If you see `error: 'SYNTH_SAMPLE_RATE' undeclared`, make sure `synth.h` from Task 5 defines it (it should).

- [ ] **Step 3: Manual smoke test — write a temporary tone-test main**

We don't have a GUI yet, so temporarily replace `src/main.c` with:
```c
#include <windows.h>
#include "audio.h"

int WINAPI WinMain(HINSTANCE hi, HINSTANCE hp, LPSTR c, int s) {
    (void)hi; (void)hp; (void)c; (void)s;
    AudioState* st = audio_init();
    if (!st) return 1;
    AudioParams p = { 440.0, 440.0, 0.3 };
    audio_set_params(st, p);
    audio_play(st);
    Sleep(2000);
    audio_stop(st);
    Sleep(200);   /* let fade-out finish */
    audio_shutdown(st);
    return 0;
}
```

- [ ] **Step 4: Build and run**

Run: `make && ./ToneGen.exe`
Expected: you hear a clean 440 Hz sine for ~2 seconds at moderate volume, with no click at start or end. The process exits cleanly.

If you hear no sound: check Windows audio output device; check `waveOutOpen` didn't pop a `MessageBox`.
If you hear clicks: the fade ramp isn't applying — confirm Task 5's gain ramp test passes.

- [ ] **Step 5: Restore the empty main**

Put `src/main.c` back to its Task-1 contents (the empty `WinMain` returning 0). Task 9 will write the real one.

- [ ] **Step 6: Commit**

```
git add src/audio.c src/main.c
git commit -m "audio: implement waveOut lifecycle with click-free fade ramps"
```

---

### Task 7: GUI module — interface, window class, Frequency group

**Goal:** Create the main window, register its class, lay out the **Frequency** group (preset combo + custom edit). Wire the preset combo to update the custom edit; wire the custom edit to push freq to audio (mono-mode behavior only for now — Mode group comes in Task 8).

**Files:**
- Modify: `src/gui.h`
- Modify: `src/gui.c`

- [ ] **Step 1: Declare the interface in `src/gui.h`**

```c
#ifndef GUI_H
#define GUI_H

#include <windows.h>
#include "audio.h"

typedef struct GuiState GuiState;

/* Creates the main window. Does not show it — caller calls ShowWindow.
 * Returns NULL on failure. */
GuiState* gui_create(HINSTANCE inst, AudioState* audio);

/* Returns the HWND of the main window. */
HWND gui_hwnd(GuiState* gs);

/* Standard Win32 message loop. Returns the WM_QUIT exit code. */
int gui_run_message_loop(GuiState* gs);

/* Releases resources. Safe with NULL. */
void gui_destroy(GuiState* gs);

#endif
```

- [ ] **Step 2: Implement window class + Frequency group**

Write `src/gui.c`:
```c
#include "gui.h"
#include "presets.h"

#include <commctrl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define ID_PRESET_COMBO   1001
#define ID_CUSTOM_EDIT    1002

#define WIN_CLASS         "ToneGenMain"

struct GuiState {
    HINSTANCE   inst;
    HWND        hwnd;
    AudioState* audio;

    HWND hPresetCombo;
    HWND hCustomEdit;

    double custom_hz;   /* what the user last entered */
};

static LRESULT CALLBACK wnd_proc(HWND, UINT, WPARAM, LPARAM);

static void push_params(GuiState* gs) {
    AudioParams p = { gs->custom_hz, gs->custom_hz, 0.5 };
    audio_set_params(gs->audio, p);
}

static void populate_presets(GuiState* gs) {
    int count = 0;
    const Preset* list = presets_list(&count);
    for (int i = 0; i < count; i++) {
        SendMessageA(gs->hPresetCombo, CB_ADDSTRING, 0, (LPARAM)list[i].name);
    }
}

static void on_preset_changed(GuiState* gs) {
    int sel = (int)SendMessageA(gs->hPresetCombo, CB_GETCURSEL, 0, 0);
    if (sel < 0) return;
    int count = 0;
    const Preset* list = presets_list(&count);
    if (sel >= count) return;
    char buf[32];
    snprintf(buf, sizeof buf, "%.3f", list[sel].hz);
    SetWindowTextA(gs->hCustomEdit, buf);
    gs->custom_hz = list[sel].hz;
    push_params(gs);
}

static void on_custom_committed(GuiState* gs) {
    char buf[32] = {0};
    GetWindowTextA(gs->hCustomEdit, buf, sizeof buf);
    double v = atof(buf);
    if (v < 0.001) v = 0.001;
    if (v > 22050.0) v = 22050.0;
    gs->custom_hz = v;
    /* re-display the clamped value */
    char fixed[32];
    snprintf(fixed, sizeof fixed, "%.3f", v);
    SetWindowTextA(gs->hCustomEdit, fixed);
    /* Drop preset selection since user typed a custom value */
    SendMessageA(gs->hPresetCombo, CB_SETCURSEL, (WPARAM)-1, 0);
    push_params(gs);
}

GuiState* gui_create(HINSTANCE inst, AudioState* audio) {
    GuiState* gs = (GuiState*)calloc(1, sizeof *gs);
    if (!gs) return NULL;
    gs->inst = inst;
    gs->audio = audio;
    gs->custom_hz = 440.0;

    WNDCLASSA wc = {0};
    wc.lpfnWndProc   = wnd_proc;
    wc.hInstance     = inst;
    wc.hCursor       = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_BTNFACE + 1);
    wc.lpszClassName = WIN_CLASS;
    if (!RegisterClassA(&wc)) { free(gs); return NULL; }

    DWORD style = WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX;
    gs->hwnd = CreateWindowA(WIN_CLASS, "Tone Generator", style,
                             CW_USEDEFAULT, CW_USEDEFAULT, 416, 360,
                             NULL, NULL, inst, gs);
    if (!gs->hwnd) { free(gs); return NULL; }
    SetWindowLongPtrA(gs->hwnd, GWLP_USERDATA, (LONG_PTR)gs);

    /* Frequency group box at top */
    CreateWindowA("BUTTON", "Frequency", WS_CHILD | WS_VISIBLE | BS_GROUPBOX,
                  10, 5, 380, 80, gs->hwnd, NULL, inst, NULL);

    CreateWindowA("STATIC", "Preset:", WS_CHILD | WS_VISIBLE,
                  25, 28, 50, 18, gs->hwnd, NULL, inst, NULL);
    gs->hPresetCombo = CreateWindowA("COMBOBOX", NULL,
                  WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST | WS_VSCROLL,
                  80, 25, 290, 200, gs->hwnd,
                  (HMENU)(LONG_PTR)ID_PRESET_COMBO, inst, NULL);

    CreateWindowA("STATIC", "Custom:", WS_CHILD | WS_VISIBLE,
                  25, 58, 50, 18, gs->hwnd, NULL, inst, NULL);
    gs->hCustomEdit = CreateWindowA("EDIT", "440.000",
                  WS_CHILD | WS_VISIBLE | WS_BORDER | ES_AUTOHSCROLL,
                  80, 55, 100, 22, gs->hwnd,
                  (HMENU)(LONG_PTR)ID_CUSTOM_EDIT, inst, NULL);
    CreateWindowA("STATIC", "Hz", WS_CHILD | WS_VISIBLE,
                  185, 58, 30, 18, gs->hwnd, NULL, inst, NULL);

    populate_presets(gs);
    push_params(gs);
    return gs;
}

HWND gui_hwnd(GuiState* gs) { return gs ? gs->hwnd : NULL; }

int gui_run_message_loop(GuiState* gs) {
    (void)gs;
    MSG msg;
    while (GetMessageA(&msg, NULL, 0, 0) > 0) {
        TranslateMessage(&msg);
        DispatchMessageA(&msg);
    }
    return (int)msg.wParam;
}

void gui_destroy(GuiState* gs) {
    if (!gs) return;
    if (gs->hwnd) DestroyWindow(gs->hwnd);
    free(gs);
}

static LRESULT CALLBACK wnd_proc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    GuiState* gs = (GuiState*)GetWindowLongPtrA(hwnd, GWLP_USERDATA);
    switch (msg) {
        case WM_COMMAND: {
            if (!gs) break;
            WORD id   = LOWORD(wp);
            WORD code = HIWORD(wp);
            if (id == ID_PRESET_COMBO && code == CBN_SELCHANGE) {
                on_preset_changed(gs);
            } else if (id == ID_CUSTOM_EDIT && code == EN_KILLFOCUS) {
                on_custom_committed(gs);
            }
            return 0;
        }
        case WM_CLOSE:
            DestroyWindow(hwnd);
            return 0;
        case WM_DESTROY:
            PostQuitMessage(0);
            return 0;
    }
    return DefWindowProcA(hwnd, msg, wp, lp);
}
```

- [ ] **Step 3: Build**

Run: `make`
Expected: clean build.

- [ ] **Step 4: Manual smoke test — write a temporary main that just shows the GUI**

Replace `src/main.c` with:
```c
#include "audio.h"
#include "gui.h"

int WINAPI WinMain(HINSTANCE inst, HINSTANCE hp, LPSTR c, int show) {
    (void)hp; (void)c;
    AudioState* a = audio_init();
    if (!a) return 1;
    GuiState* g = gui_create(inst, a);
    if (!g) { audio_shutdown(a); return 1; }
    ShowWindow(gui_hwnd(g), show);
    UpdateWindow(gui_hwnd(g));
    int rc = gui_run_message_loop(g);
    gui_destroy(g);
    audio_shutdown(a);
    return rc;
}
```

- [ ] **Step 5: Build and run**

Run: `make && ./ToneGen.exe`
Expected: a window titled "Tone Generator" appears with classic gray bevels. The Frequency group contains a preset dropdown listing every preset from Task 3 and a "Custom: 440.000 Hz" edit field. Selecting a preset updates the edit field. Typing into the edit field and tabbing/clicking away clamps + reformats the value. **No tone is yet audible** because we removed `audio_play()` — Task 8 wires the Play button.

- [ ] **Step 6: Commit**

```
git add src/gui.h src/gui.c src/main.c
git commit -m "gui: add main window with Frequency group (preset + custom)"
```

---

### Task 8: GUI — Mode group + Output group (Play / Stop / Volume)

**Goal:** Add the **Mode** group (Single/Binaural radios, Base/Beat edits, Advanced checkbox, L/R edits) and the **Output** group (Volume trackbar, Play/Stop buttons, Status label). Wire everything to `audio_set_params`, `audio_play`, `audio_stop`.

**Files:**
- Modify: `src/gui.c`

- [ ] **Step 1: Add new control IDs near the top**

Add below the existing `#define ID_CUSTOM_EDIT 1002`:
```c
#define ID_MODE_SINGLE    1003
#define ID_MODE_BINAURAL  1004
#define ID_BASE_EDIT      1005
#define ID_BEAT_EDIT      1006
#define ID_ADV_CHECK      1007
#define ID_LEFT_EDIT      1008
#define ID_RIGHT_EDIT     1009
#define ID_VOLUME_TRACK   1010
#define ID_PLAY_BUTTON    1011
#define ID_STOP_BUTTON    1012
```

- [ ] **Step 2: Extend `GuiState`**

Replace the `GuiState` struct with:
```c
struct GuiState {
    HINSTANCE   inst;
    HWND        hwnd;
    AudioState* audio;

    HWND hPresetCombo;
    HWND hCustomEdit;
    HWND hModeSingle, hModeBinaural;
    HWND hBaseEdit, hBeatEdit;
    HWND hAdvCheck;
    HWND hLeftEdit, hRightEdit;
    HWND hVolumeTrack, hVolumeLabel;
    HWND hPlayBtn, hStopBtn;
    HWND hStatusLabel;

    double custom_hz;
    double base_hz, beat_hz;
    double left_hz, right_hz;
    int    binaural;     /* 0 = single, 1 = binaural */
    int    advanced;     /* 0 = base+beat, 1 = L/R direct */
    int    volume_pct;   /* 0..100 */
};
```

- [ ] **Step 3: Replace `push_params` and add helpers**

Replace the existing `push_params` with the version below and add the new helpers above it:

```c
static void recompute_lr_from_base_beat(GuiState* gs) {
    gs->left_hz  = gs->base_hz;
    gs->right_hz = gs->base_hz + gs->beat_hz;
}

static void update_lr_display(GuiState* gs) {
    char b[32];
    snprintf(b, sizeof b, "%.3f", gs->left_hz);  SetWindowTextA(gs->hLeftEdit,  b);
    snprintf(b, sizeof b, "%.3f", gs->right_hz); SetWindowTextA(gs->hRightEdit, b);
}

static void apply_enabled_states(GuiState* gs) {
    /* Custom edit is active only in single mode */
    EnableWindow(gs->hCustomEdit, !gs->binaural);
    /* Mode group's children active only in binaural */
    EnableWindow(gs->hBaseEdit,  gs->binaural && !gs->advanced);
    EnableWindow(gs->hBeatEdit,  gs->binaural && !gs->advanced);
    EnableWindow(gs->hLeftEdit,  gs->binaural &&  gs->advanced);
    EnableWindow(gs->hRightEdit, gs->binaural &&  gs->advanced);
    EnableWindow(gs->hAdvCheck,  gs->binaural);
}

static void update_status(GuiState* gs) {
    const char* base = audio_is_playing(gs->audio) ? "Playing" : "Idle";
    char msg[128];
    if (!gs->binaural && (gs->custom_hz < 20.0 || gs->custom_hz > 20000.0)) {
        snprintf(msg, sizeof msg,
                 "Status: %s  (note: %.2f Hz is outside audible range)",
                 base, gs->custom_hz);
    } else {
        snprintf(msg, sizeof msg, "Status: %s", base);
    }
    SetWindowTextA(gs->hStatusLabel, msg);
}

static void push_params(GuiState* gs) {
    AudioParams p;
    if (gs->binaural) {
        p.left_hz  = gs->left_hz;
        p.right_hz = gs->right_hz;
    } else {
        p.left_hz = p.right_hz = gs->custom_hz;
    }
    p.volume = (double)gs->volume_pct / 100.0;
    audio_set_params(gs->audio, p);
    update_status(gs);
}
```

- [ ] **Step 4: Update `on_preset_changed`**

Replace the existing `on_preset_changed` with:
```c
static void on_preset_changed(GuiState* gs) {
    int sel = (int)SendMessageA(gs->hPresetCombo, CB_GETCURSEL, 0, 0);
    if (sel < 0) return;
    int count = 0;
    const Preset* list = presets_list(&count);
    if (sel >= count) return;
    double hz = list[sel].hz;
    if (gs->binaural) {
        gs->base_hz = hz;
        char b[32]; snprintf(b, sizeof b, "%.3f", hz);
        SetWindowTextA(gs->hBaseEdit, b);
        if (!gs->advanced) { recompute_lr_from_base_beat(gs); update_lr_display(gs); }
    } else {
        gs->custom_hz = hz;
        char b[32]; snprintf(b, sizeof b, "%.3f", hz);
        SetWindowTextA(gs->hCustomEdit, b);
    }
    push_params(gs);
}
```

- [ ] **Step 5: Add commit handlers for the new edits**

Add these helpers just after `on_custom_committed`:
```c
/* Returns 1 if input is a valid float in [0.0, 22050.0]. Writes the parsed
 * (and reformatted) value to *out. If invalid, restores the edit field to
 * the prior_value and returns 0 (caller should NOT push to audio). */
static int read_clamped(HWND edit, double prior_value, double* out) {
    char b[32] = {0};
    GetWindowTextA(edit, b, sizeof b);
    char* end = NULL;
    double v = strtod(b, &end);
    int valid = (end != b) && (v >= 0.0) && (v <= 22050.0);
    if (!valid) {
        char fixed[32]; snprintf(fixed, sizeof fixed, "%.3f", prior_value);
        SetWindowTextA(edit, fixed);
        return 0;
    }
    char fixed[32]; snprintf(fixed, sizeof fixed, "%.3f", v);
    SetWindowTextA(edit, fixed);
    *out = v;
    return 1;
}

static void on_base_committed(GuiState* gs) {
    double v;
    if (!read_clamped(gs->hBaseEdit, gs->base_hz, &v)) return;
    gs->base_hz = v;
    if (!gs->advanced) { recompute_lr_from_base_beat(gs); update_lr_display(gs); }
    push_params(gs);
}
static void on_beat_committed(GuiState* gs) {
    double v;
    if (!read_clamped(gs->hBeatEdit, gs->beat_hz, &v)) return;
    gs->beat_hz = v;
    if (!gs->advanced) { recompute_lr_from_base_beat(gs); update_lr_display(gs); }
    push_params(gs);
}
static void on_left_committed(GuiState* gs) {
    double v;
    if (!read_clamped(gs->hLeftEdit, gs->left_hz, &v)) return;
    gs->left_hz = v;
    push_params(gs);
}
static void on_right_committed(GuiState* gs) {
    double v;
    if (!read_clamped(gs->hRightEdit, gs->right_hz, &v)) return;
    gs->right_hz = v;
    push_params(gs);
}
```

- [ ] **Step 6: Add mode-toggle handlers**

```c
static void on_mode_changed(GuiState* gs, int binaural) {
    gs->binaural = binaural;
    apply_enabled_states(gs);
    push_params(gs);
}

static void on_advanced_toggled(GuiState* gs) {
    gs->advanced = (SendMessageA(gs->hAdvCheck, BM_GETCHECK, 0, 0) == BST_CHECKED);
    apply_enabled_states(gs);
    if (!gs->advanced) { recompute_lr_from_base_beat(gs); update_lr_display(gs); }
    push_params(gs);
}

static void on_volume_changed(GuiState* gs) {
    gs->volume_pct = (int)SendMessageA(gs->hVolumeTrack, TBM_GETPOS, 0, 0);
    char b[16]; snprintf(b, sizeof b, "%d %%", gs->volume_pct);
    SetWindowTextA(gs->hVolumeLabel, b);
    push_params(gs);
}
```

- [ ] **Step 7: Initialise new fields in `gui_create` and create the controls**

Inside `gui_create`, after the line `gs->custom_hz = 440.0;`, add:
```c
    gs->base_hz    = 100.0;
    gs->beat_hz    = 7.83;
    gs->left_hz    = 100.0;
    gs->right_hz   = 107.83;
    gs->binaural   = 0;
    gs->advanced   = 0;
    gs->volume_pct = 50;
```

Then, just before the line `populate_presets(gs);`, insert all the new controls:
```c
    /* Mode group */
    CreateWindowA("BUTTON", "Mode", WS_CHILD | WS_VISIBLE | BS_GROUPBOX,
                  10, 95, 380, 130, gs->hwnd, NULL, inst, NULL);

    gs->hModeSingle = CreateWindowA("BUTTON", "Single tone",
                  WS_CHILD | WS_VISIBLE | BS_AUTORADIOBUTTON | WS_GROUP,
                  25, 115, 100, 20, gs->hwnd,
                  (HMENU)(LONG_PTR)ID_MODE_SINGLE, inst, NULL);
    gs->hModeBinaural = CreateWindowA("BUTTON", "Binaural",
                  WS_CHILD | WS_VISIBLE | BS_AUTORADIOBUTTON,
                  140, 115, 100, 20, gs->hwnd,
                  (HMENU)(LONG_PTR)ID_MODE_BINAURAL, inst, NULL);
    SendMessageA(gs->hModeSingle, BM_SETCHECK, BST_CHECKED, 0);

    CreateWindowA("STATIC", "Base:", WS_CHILD | WS_VISIBLE,
                  25, 145, 35, 18, gs->hwnd, NULL, inst, NULL);
    gs->hBaseEdit = CreateWindowA("EDIT", "100.000",
                  WS_CHILD | WS_VISIBLE | WS_BORDER | ES_AUTOHSCROLL,
                  65, 142, 80, 22, gs->hwnd,
                  (HMENU)(LONG_PTR)ID_BASE_EDIT, inst, NULL);
    CreateWindowA("STATIC", "Hz   Beat:", WS_CHILD | WS_VISIBLE,
                  150, 145, 70, 18, gs->hwnd, NULL, inst, NULL);
    gs->hBeatEdit = CreateWindowA("EDIT", "7.830",
                  WS_CHILD | WS_VISIBLE | WS_BORDER | ES_AUTOHSCROLL,
                  225, 142, 80, 22, gs->hwnd,
                  (HMENU)(LONG_PTR)ID_BEAT_EDIT, inst, NULL);
    CreateWindowA("STATIC", "Hz", WS_CHILD | WS_VISIBLE,
                  310, 145, 25, 18, gs->hwnd, NULL, inst, NULL);

    gs->hAdvCheck = CreateWindowA("BUTTON", "Advanced (set L/R directly)",
                  WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX,
                  25, 170, 250, 20, gs->hwnd,
                  (HMENU)(LONG_PTR)ID_ADV_CHECK, inst, NULL);

    CreateWindowA("STATIC", "L:", WS_CHILD | WS_VISIBLE,
                  25, 197, 15, 18, gs->hwnd, NULL, inst, NULL);
    gs->hLeftEdit = CreateWindowA("EDIT", "100.000",
                  WS_CHILD | WS_VISIBLE | WS_BORDER | ES_AUTOHSCROLL,
                  45, 194, 80, 22, gs->hwnd,
                  (HMENU)(LONG_PTR)ID_LEFT_EDIT, inst, NULL);
    CreateWindowA("STATIC", "Hz   R:", WS_CHILD | WS_VISIBLE,
                  130, 197, 50, 18, gs->hwnd, NULL, inst, NULL);
    gs->hRightEdit = CreateWindowA("EDIT", "107.830",
                  WS_CHILD | WS_VISIBLE | WS_BORDER | ES_AUTOHSCROLL,
                  185, 194, 80, 22, gs->hwnd,
                  (HMENU)(LONG_PTR)ID_RIGHT_EDIT, inst, NULL);
    CreateWindowA("STATIC", "Hz", WS_CHILD | WS_VISIBLE,
                  270, 197, 25, 18, gs->hwnd, NULL, inst, NULL);

    /* Output group */
    CreateWindowA("BUTTON", "Output", WS_CHILD | WS_VISIBLE | BS_GROUPBOX,
                  10, 235, 380, 95, gs->hwnd, NULL, inst, NULL);

    CreateWindowA("STATIC", "Volume:", WS_CHILD | WS_VISIBLE,
                  25, 258, 50, 18, gs->hwnd, NULL, inst, NULL);
    INITCOMMONCONTROLSEX icc = { sizeof icc, ICC_BAR_CLASSES };
    InitCommonControlsEx(&icc);
    gs->hVolumeTrack = CreateWindowA(TRACKBAR_CLASSA, NULL,
                  WS_CHILD | WS_VISIBLE | TBS_HORZ | TBS_NOTICKS,
                  80, 255, 220, 24, gs->hwnd,
                  (HMENU)(LONG_PTR)ID_VOLUME_TRACK, inst, NULL);
    SendMessageA(gs->hVolumeTrack, TBM_SETRANGE, TRUE, MAKELONG(0, 100));
    SendMessageA(gs->hVolumeTrack, TBM_SETPOS,   TRUE, gs->volume_pct);
    gs->hVolumeLabel = CreateWindowA("STATIC", "50 %",
                  WS_CHILD | WS_VISIBLE,
                  310, 258, 60, 18, gs->hwnd, NULL, inst, NULL);

    gs->hPlayBtn = CreateWindowA("BUTTON", "Play",
                  WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                  80, 285, 90, 26, gs->hwnd,
                  (HMENU)(LONG_PTR)ID_PLAY_BUTTON, inst, NULL);
    gs->hStopBtn = CreateWindowA("BUTTON", "Stop",
                  WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                  185, 285, 90, 26, gs->hwnd,
                  (HMENU)(LONG_PTR)ID_STOP_BUTTON, inst, NULL);

    gs->hStatusLabel = CreateWindowA("STATIC", "Status: Idle",
                  WS_CHILD | WS_VISIBLE,
                  25, 318, 360, 18, gs->hwnd, NULL, inst, NULL);

    apply_enabled_states(gs);
```

- [ ] **Step 8: Extend `WM_COMMAND` dispatch and add `WM_HSCROLL`**

Replace the `WM_COMMAND` case in `wnd_proc` with:
```c
        case WM_COMMAND: {
            if (!gs) break;
            WORD id   = LOWORD(wp);
            WORD code = HIWORD(wp);
            switch (id) {
                case ID_PRESET_COMBO:   if (code == CBN_SELCHANGE) on_preset_changed(gs); break;
                case ID_CUSTOM_EDIT:    if (code == EN_KILLFOCUS)  on_custom_committed(gs); break;
                case ID_BASE_EDIT:      if (code == EN_KILLFOCUS)  on_base_committed(gs); break;
                case ID_BEAT_EDIT:      if (code == EN_KILLFOCUS)  on_beat_committed(gs); break;
                case ID_LEFT_EDIT:      if (code == EN_KILLFOCUS)  on_left_committed(gs); break;
                case ID_RIGHT_EDIT:     if (code == EN_KILLFOCUS)  on_right_committed(gs); break;
                case ID_MODE_SINGLE:    if (code == BN_CLICKED)    on_mode_changed(gs, 0); break;
                case ID_MODE_BINAURAL:  if (code == BN_CLICKED)    on_mode_changed(gs, 1); break;
                case ID_ADV_CHECK:      if (code == BN_CLICKED)    on_advanced_toggled(gs); break;
                case ID_PLAY_BUTTON:    if (code == BN_CLICKED) {
                    push_params(gs);
                    audio_play(gs->audio);
                    update_status(gs);
                } break;
                case ID_STOP_BUTTON:    if (code == BN_CLICKED) {
                    audio_stop(gs->audio);
                    update_status(gs);
                } break;
            }
            return 0;
        }
```

Then, *after* the `WM_COMMAND` case (still inside the `switch (msg)`), add:
```c
        case WM_HSCROLL: {
            if (gs && (HWND)lp == gs->hVolumeTrack) on_volume_changed(gs);
            return 0;
        }
```

- [ ] **Step 9: Build**

Run: `make`
Expected: clean build.

- [ ] **Step 10: Manual smoke test — full GUI walk-through**

Run: `./ToneGen.exe`

Verify against the spec's manual checklist:

1. Window opens with classic gray bevels (no flat Win11 theming) and three group boxes laid out vertically.
2. Single is selected; Base/Beat/L/R are greyed; Custom is editable.
3. Pick "Concert A (standard)" → Custom shows 440.000 → click Play → a clean 440 Hz tone fades in. No click.
4. Drag volume slider → loudness changes smoothly in real time.
5. Type 432 into Custom, click away → tone shifts to 432 Hz with no click. Status label still says "Playing".
6. Click Stop → tone fades out, no click. Status returns to "Idle".
7. Switch to Binaural → Custom greys out, Base/Beat become active, L/R remain greyed, Advanced becomes available.
8. Set Base=100, Beat=7.83 → L auto-updates to 100, R to 107.83. Play with headphones → wobble sensation around 7.83 Hz.
9. Check Advanced → Base/Beat grey out, L/R become editable. Edit L=120, R=130 → live tones change.
10. Pick "Schumann resonance" from preset → in Single mode the Custom field becomes 7.830 and the status label notes it's outside audible range.
11. Close window → tone fades out cleanly, no crash.

- [ ] **Step 11: Commit**

```
git add src/gui.c
git commit -m "gui: add Mode and Output groups; wire Play/Stop/Volume to audio"
```

---

### Task 9: Final main + polish

**Goal:** Promote the smoke-test `main.c` from Task 8 to the final `main.c` (it's already correct). Add the `ICC_STANDARD_CLASSES` init that some Win11 builds need for the radio buttons to display correctly. Sanity-check the whole thing.

**Files:**
- Modify: `src/main.c` (verify only — should already be correct)
- Modify: `src/gui.c` (one small init tweak)

- [ ] **Step 1: Verify `main.c`**

Confirm `src/main.c` matches what Task 8 Step 4 wrote. If not, restore it.

- [ ] **Step 2: Initialise common controls earlier, with standard classes**

In `src/gui.c`, the current Task 8 code calls `InitCommonControlsEx` mid-`gui_create`, AFTER many child controls have already been created. That works on modern Windows because `comctl32` is normally pre-loaded, but it is fragile. Two changes:

(a) Change the flags to register both bar classes (trackbar) and standard classes (buttons/edits/combo) explicitly:
```c
INITCOMMONCONTROLSEX icc = { sizeof icc, ICC_BAR_CLASSES | ICC_STANDARD_CLASSES };
```

(b) Move the `INITCOMMONCONTROLSEX icc = ...; InitCommonControlsEx(&icc);` pair to the very top of `gui_create`, immediately after the `calloc` + the `gs->inst = inst; gs->audio = audio;` assignments and BEFORE the first `CreateWindowA` call. Delete the now-redundant pair from its old location near the Output group.

The result: common controls are guaranteed registered before any child window is created.

- [ ] **Step 3: Build clean from scratch**

Run: `make clean && make && make test`
Expected:
- Build produces `ToneGen.exe` with no warnings.
- `test_synth`: 10 passed.
- `test_presets`: N presets, 3 named lookups passed.

- [ ] **Step 4: Full manual run-through**

Run: `./ToneGen.exe`

Walk through the entire spec's "Testing" section (9 items) once more end-to-end. Every item must pass before you mark this task complete.

- [ ] **Step 5: Commit**

```
git add src/gui.c
git commit -m "gui: register standard common control classes for non-manifested run"
```

---

## Self-review (already performed by author)

- **Spec coverage:** Architecture (Tasks 4+6), pure synthesis (Tasks 2+5), waveOut + click-free fades (Task 6), Frequency group (Task 7), Mode group + Advanced toggle (Task 8), Output group with Play/Stop/Volume (Task 8), preset table (Task 3), Single ↔ Binaural enable-rules (Task 8 `apply_enabled_states`), sub-audible warning (Task 8 `update_status`), Nyquist clamp (Tasks 7+8 `read_clamped`/`on_custom_committed`), classic-look (Tasks 1+7 — no manifest), C99 + MinGW build (Task 1). ✓
- **Placeholder scan:** No TBD / "add validation" / "similar to" / "implement later". The two `LEARNING SPOT` markers are explicit user-contribution points with full surrounding scaffolding and a fall-back hint paragraph. ✓
- **Type consistency:** `AudioParams` { left_hz, right_hz, volume } used identically in Tasks 4, 6, 7, 8. `SynthFrameParams` field names match between Task 5's header and Task 6's caller. `synth_next_gain`, `synth_fill_buffer`, `presets_list` signatures are stable across all references. ✓
