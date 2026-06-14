# Binaural Carrier / Beat Split — Design

**Date:** 2026-06-14
**Status:** Approved (pending implementation plan)

## Problem

In binaural mode the app lets you set a **Base** frequency (the carrier — the
pitch both ears converge on) and a **Beat** frequency (the small `|R − L|`
difference your brain entrains to). The README already calls Base "the carrier
or base" (README.md:70).

The single **Binaural preset** dropdown (`presets.c` `BINAURAL_PRESETS[]`) is
physiologically incorrect: it lists Solfeggio/tuning frequencies (174–963 Hz)
as **beat** values with a fixed `base = 200`. A 528 Hz "beat" produces
left = 200 / right = 728 — two unrelated tones, not a perceivable binaural beat.
This contradicts the README's own rule (line 110) that the beat must stay below
~40 Hz. There is also no way to combine a Solfeggio **carrier** with a brainwave
**beat**, which is the physically correct way to use those frequencies.

## Goal

Split the one dropdown into two independent pickers — **Carrier** and **Beat** —
so a user selects, e.g., a 963 Hz Solfeggio carrier *and* a 40 Hz Gamma beat
together. Remove the incorrect Solfeggio-as-beat presets.

## Non-Goals

- No change to the audio engine (`synth.c`, `audio.c`). Carrier already maps to
  `base_hz`; this is purely a presets + GUI + docs change.
- No change to Single-tone or Noise modes, or to the Advanced (direct L/R) path
  beyond enabling/disabling the new combos.

## Design

### 1. Data model — `presets.h` / `presets.c`

Delete `BinauralPreset` and `binaural_presets_list()`. Reuse the existing
`Preset { const char* name; double hz; }` type for two new static lists with
accessors mirroring `presets_list()`:

```c
const Preset* binaural_carrier_list(int* out_count);
const Preset* binaural_beat_list(int* out_count);
```

**Carrier list** (the pitch you consciously hear):

| Name              | Hz     |
|-------------------|--------|
| 200 (neutral)     | 200.00 |
| Solfeggio 174     | 174.00 |
| Solfeggio 285     | 285.00 |
| Solfeggio UT 396  | 396.00 |
| Solfeggio RE 417  | 417.00 |
| Solfeggio MI 528  | 528.00 |
| Solfeggio FA 639  | 639.00 |
| Solfeggio SOL 741 | 741.00 |
| Solfeggio LA 852  | 852.00 |
| Solfeggio 963     | 963.00 |
| Middle C 261.63   | 261.63 |
| Concert A 432     | 432.00 |
| Concert A 440     | 440.00 |

**Beat list** (the entrainment difference, `|R − L|`):

| Name              | Hz    |
|-------------------|-------|
| Delta 2 Hz        | 2.00  |
| Theta 6 Hz        | 6.00  |
| Schumann 7.83 Hz  | 7.83  |
| Alpha 10 Hz       | 10.00 |
| SMR 14 Hz         | 14.00 |
| Beta 20 Hz        | 20.00 |
| Gamma 40 Hz       | 40.00 |

### 2. UI — `gui.c`

Replace the single `hBinauralCombo` (ID_BINAURAL_COMBO) with two combos:
`hCarrierCombo` (ID_CARRIER_COMBO) and `hBeatCombo` (ID_BEAT_COMBO). Each combo's
**index 0 is `— Custom —`**; preset entries follow at index `i+1`.

**Layout:** two stacked rows inside the Mode group box — `Carrier:` row then
`Beat:` row — replacing the one preset row at y≈139. Everything below the new
rows (Base/Beat edits, Advanced checkbox, L/R edits) and every group/control
below the Mode group shifts down by one row height (~28 px). The Mode group box
height and the main window height grow by ~28 px accordingly. Exact coordinates
are an implementation detail for the plan.

### 3. Smart-sync behavior

- **Preset → fields:** On `CBN_SELCHANGE`, if the selected index is 0 (`— Custom —`)
  do nothing. Otherwise write the chosen `hz` into `base_hz` (carrier) or
  `beat_hz` (beat), update the corresponding edit field, and — when not in
  advanced mode — `recompute_lr_from_base_beat()` + `update_lr_display()`.
- **Fields → preset (honest reflection):** Add a helper that, given the current
  `base_hz` / `beat_hz`, selects the matching preset within a **0.01 Hz**
  tolerance, or selects `— Custom —` if none matches. Call it after every change
  to those values: manual Base/Beat commits (`on_base_committed`,
  `on_beat_committed`), preset selection, and startup init.
- **Enable/disable:** Both combos are enabled only in binaural mode, not noise,
  not band-check, and not advanced — i.e. the same predicate the existing
  `hBeatEdit` uses in `sync_enable_states()`.

### 4. Defaults

Start self-consistent so both combos show a real selection (not Custom):
Carrier = `200 (neutral)`, Beat = `Schumann 7.83 Hz` → `base_hz = 200`,
`beat_hz = 7.83`, `left = 200`, `right = 207.83`. This changes the current
startup Base from 100 → 200.

### 5. Documentation

- `README.md`: update the binaural dropdown description (line ~33) to describe
  the two dropdowns; correct the "use Solfeggio frequencies as the **beat**"
  guidance (line ~118) to "as the **carrier**, with a brainwave beat layered on."
- `ReadMe.txt`: mirror the same corrections.

### 6. Tests — `tests/test_presets.c`

- Assert both new lists are non-NULL with expected counts and a few named
  lookups (e.g. carrier "Solfeggio 963" = 963.0, beat "Gamma 40 Hz" = 40.0).
- **Physics invariant guards** (codify the fix so it can't regress):
  - every entry in `binaural_beat_list()` has `hz ≤ 40.0`;
  - every entry in `binaural_carrier_list()` has `hz ≥ 20.0`.

## Risks

- Removing `binaural_presets_list()` breaks any other caller. Only `gui.c` uses
  it; the plan must grep to confirm before deletion.
- Window/control reflow: hard-coded y-coordinates below the Mode group must all
  shift consistently, or controls overlap. The plan should enumerate them.
