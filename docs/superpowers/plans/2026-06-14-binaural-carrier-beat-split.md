# Binaural Carrier / Beat Split Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Replace the single (physically incorrect) binaural preset dropdown with two independent pickers — **Carrier** and **Beat** — so a user can combine a Solfeggio carrier with a brainwave beat.

**Architecture:** Pure data + GUI change; the audio engine is untouched (carrier already maps to `base_hz`). The presets module gains two `Preset[]` lists and a unit-testable tolerance-matching helper. The Win32 GUI replaces one combo with two, wires one-shot preset→field selection, and adds "smart sync" that re-selects the matching preset (or `-- Custom --`) after any field edit.

**Tech Stack:** C99, Win32 (waveOut/ComCtl32), GCC 15.2 + GNU Make. Build: `make`. Tests: `make test`.

---

## File Structure

- `src/presets.h` — add `binaural_carrier_list()`, `binaural_beat_list()`, `presets_match_index()`; remove `BinauralPreset` / `binaural_presets_list()` (Task 3).
- `src/presets.c` — implement the two lists + matcher; remove old binaural preset array.
- `tests/test_presets.c` — add coverage for both lists, the physics invariants, and the matcher.
- `src/gui.c` — replace `hBinauralCombo` with `hCarrierCombo` + `hBeatCombo`; add sync helpers; reflow layout; new defaults.
- `README.md`, `ReadMe.txt` — fix the dropdown description.

**Commit discipline:** every task leaves both `make` and `make test` green. Task 1 adds new code without removing old, Task 2 migrates the GUI, Task 3 removes the now-dead old code.

---

## Task 1: Presets data model + matcher (TDD)

**Files:**
- Modify: `src/presets.h`
- Modify: `src/presets.c`
- Test: `tests/test_presets.c`

- [ ] **Step 1: Write the failing tests**

In `tests/test_presets.c`, add the following inside `main()`, immediately before the final `printf(...)` line:

```c
    /* --- Binaural carrier list --- */
    int ccount = 0;
    const Preset* carriers = binaural_carrier_list(&ccount);
    assert(carriers != 0);
    assert(ccount == 13);
    const Preset* c963 = 0;
    for (int i = 0; i < ccount; i++)
        if (strcmp(carriers[i].name, "Solfeggio 963") == 0) c963 = &carriers[i];
    assert(c963 != 0 && c963->hz == 963.0);
    /* invariant: every carrier is audible (>= 20 Hz) */
    for (int i = 0; i < ccount; i++) assert(carriers[i].hz >= 20.0);

    /* --- Binaural beat list --- */
    int bcount = 0;
    const Preset* beats = binaural_beat_list(&bcount);
    assert(beats != 0);
    assert(bcount == 7);
    const Preset* gamma = 0;
    for (int i = 0; i < bcount; i++)
        if (strcmp(beats[i].name, "Gamma 40 Hz") == 0) gamma = &beats[i];
    assert(gamma != 0 && gamma->hz == 40.0);
    /* physics invariant: every beat is a perceivable binaural beat (<= 40 Hz) */
    for (int i = 0; i < bcount; i++) assert(beats[i].hz <= 40.0);

    /* --- Tolerance matcher --- */
    assert(presets_match_index(beats, bcount, 7.83,  0.01) == 2);   /* Schumann */
    assert(presets_match_index(beats, bcount, 7.835, 0.01) == 2);   /* within tol */
    assert(presets_match_index(beats, bcount, 8.0,   0.01) == -1);  /* no match */
    assert(presets_match_index(carriers, ccount, 200.0, 0.01) == 0);/* neutral */
```

- [ ] **Step 2: Run the tests to verify they fail to compile**

Run: `make test_presets.exe`
Expected: FAIL — `implicit declaration of function 'binaural_carrier_list'` (and the other two new symbols).

- [ ] **Step 3: Declare the new API in `src/presets.h`**

Add these declarations after the existing `presets_list()` declaration (after line 11, before the `BinauralPreset` block):

```c
/* Binaural-mode carrier presets (the audible pitch sent to both ears). */
const Preset* binaural_carrier_list(int* out_count);

/* Binaural-mode beat presets (the |R-L| difference the brain entrains to). */
const Preset* binaural_beat_list(int* out_count);

/* Returns the index into `list` whose hz is within `tol` of `hz`,
 * or -1 if none matches. First match wins. */
int presets_match_index(const Preset* list, int count, double hz, double tol);
```

(Leave the existing `BinauralPreset` / `binaural_presets_list` lines in place for now; Task 3 removes them.)

- [ ] **Step 4: Implement in `src/presets.c`**

Add the following just before the existing `BINAURAL_PRESETS` array definition (i.e. after the `presets_list()` function, around line 24):

```c
static const Preset BINAURAL_CARRIERS[] = {
    { "200 (neutral)",     200.00 },
    { "Solfeggio 174",     174.00 },
    { "Solfeggio 285",     285.00 },
    { "Solfeggio UT 396",  396.00 },
    { "Solfeggio RE 417",  417.00 },
    { "Solfeggio MI 528",  528.00 },
    { "Solfeggio FA 639",  639.00 },
    { "Solfeggio SOL 741", 741.00 },
    { "Solfeggio LA 852",  852.00 },
    { "Solfeggio 963",     963.00 },
    { "Middle C 261.63",   261.63 },
    { "Concert A 432",     432.00 },
    { "Concert A 440",     440.00 },
};

static const Preset BINAURAL_BEATS[] = {
    { "Delta 2 Hz",       2.00  },
    { "Theta 6 Hz",       6.00  },
    { "Schumann 7.83 Hz", 7.83  },
    { "Alpha 10 Hz",      10.00 },
    { "SMR 14 Hz",        14.00 },
    { "Beta 20 Hz",       20.00 },
    { "Gamma 40 Hz",      40.00 },
};

const Preset* binaural_carrier_list(int* out_count) {
    *out_count = (int)(sizeof BINAURAL_CARRIERS / sizeof BINAURAL_CARRIERS[0]);
    return BINAURAL_CARRIERS;
}

const Preset* binaural_beat_list(int* out_count) {
    *out_count = (int)(sizeof BINAURAL_BEATS / sizeof BINAURAL_BEATS[0]);
    return BINAURAL_BEATS;
}

int presets_match_index(const Preset* list, int count, double hz, double tol) {
    for (int i = 0; i < count; i++) {
        double d = list[i].hz - hz;
        if (d < 0.0) d = -d;
        if (d <= tol) return i;
    }
    return -1;
}
```

- [ ] **Step 5: Run the tests to verify they pass**

Run: `make test`
Expected: PASS — `test_synth` output, then `test_presets: 14 presets, 3 named lookups passed` (the single-mode count is unchanged; the new asserts pass silently).

- [ ] **Step 6: Verify the full app still builds**

Run: `make`
Expected: builds `ToneGen.exe` with no errors (old `binaural_presets_list` still present, so `gui.c` is unaffected).

- [ ] **Step 7: Commit**

```bash
git add src/presets.h src/presets.c tests/test_presets.c
git commit -m "presets: add carrier/beat lists and tolerance matcher"
```

---

## Task 2: GUI — two combos, smart sync, layout reflow

**Files:**
- Modify: `src/gui.c`

No automated test (Win32 GUI); verification is a build + manual smoke test at the end. The risky matching logic is already unit-tested via `presets_match_index` in Task 1.

- [ ] **Step 1: Swap the control IDs**

In `src/gui.c`, replace this line (line 24):

```c
#define ID_BINAURAL_COMBO 1015
```

with:

```c
#define ID_CARRIER_COMBO  1015
#define ID_BEAT_COMBO     1018
```

(1016/1017 are taken by the Brain Cleaner buttons; 1018 is free.)

- [ ] **Step 2: Swap the HWND field**

In `struct GuiState`, replace (line 43):

```c
    HWND hBinauralCombo;
```

with:

```c
    HWND hCarrierCombo, hBeatCombo;
```

- [ ] **Step 3: Add the sync helpers**

Insert these two functions immediately after `update_lr_display()` (after line 74, before `apply_enabled_states`):

```c
/* Select the combo entry matching `hz`. Index 0 is "-- Custom --";
 * presets occupy indices 1..count. Falls back to "-- Custom --". */
static void sync_combo(HWND combo, const Preset* list, int count, double hz) {
    int idx = presets_match_index(list, count, hz, 0.01);
    SendMessageA(combo, CB_SETCURSEL, (WPARAM)(idx < 0 ? 0 : idx + 1), 0);
}

/* Re-point both binaural combos at whatever base_hz/beat_hz currently hold.
 * CB_SETCURSEL does not emit CBN_SELCHANGE, so this never recurses. */
static void sync_binaural_combos(GuiState* gs) {
    int cc = 0; const Preset* cl = binaural_carrier_list(&cc);
    int bc = 0; const Preset* bl = binaural_beat_list(&bc);
    sync_combo(gs->hCarrierCombo, cl, cc, gs->base_hz);
    sync_combo(gs->hBeatCombo,    bl, bc, gs->beat_hz);
}
```

- [ ] **Step 4: Update enable-state logic**

In `apply_enabled_states()`, remove this line (line 85):

```c
    if (gs->hBinauralCombo)  EnableWindow(gs->hBinauralCombo,  !bc && bn);
```

and add, immediately after the `EnableWindow(gs->hModeNoise, ...)` line:

```c
    EnableWindow(gs->hCarrierCombo, !bc && !nm && bn && !gs->advanced);
    EnableWindow(gs->hBeatCombo,    !bc && !nm && bn && !gs->advanced);
```

- [ ] **Step 5: Re-sync combos after manual Base/Beat edits**

In `on_base_committed()` (line ~204), add `sync_binaural_combos(gs);` after the recompute, so it becomes:

```c
static void on_base_committed(GuiState* gs) {
    double v;
    if (!read_clamped(gs->hBaseEdit, gs->base_hz, &v)) return;
    gs->base_hz = v;
    if (!gs->advanced) { recompute_lr_from_base_beat(gs); update_lr_display(gs); }
    sync_binaural_combos(gs);
    push_params(gs);
}
```

In `on_beat_committed()` (line ~211), the same:

```c
static void on_beat_committed(GuiState* gs) {
    double v;
    if (!read_clamped(gs->hBeatEdit, gs->beat_hz, &v)) return;
    gs->beat_hz = v;
    if (!gs->advanced) { recompute_lr_from_base_beat(gs); update_lr_display(gs); }
    sync_binaural_combos(gs);
    push_params(gs);
}
```

- [ ] **Step 6: Replace the preset-selection handler**

Delete `on_binaural_preset_changed()` entirely (lines ~247-260) and replace it with:

```c
static void on_carrier_preset_changed(GuiState* gs) {
    int sel = (int)SendMessageA(gs->hCarrierCombo, CB_GETCURSEL, 0, 0);
    if (sel <= 0) return;                 /* 0 = "-- Custom --" */
    int count = 0;
    const Preset* list = binaural_carrier_list(&count);
    if (sel - 1 >= count) return;
    gs->base_hz = list[sel - 1].hz;
    char b[32]; snprintf(b, sizeof b, "%.3f", gs->base_hz);
    SetWindowTextA(gs->hBaseEdit, b);
    if (!gs->advanced) { recompute_lr_from_base_beat(gs); update_lr_display(gs); }
    push_params(gs);
}

static void on_beat_preset_changed(GuiState* gs) {
    int sel = (int)SendMessageA(gs->hBeatCombo, CB_GETCURSEL, 0, 0);
    if (sel <= 0) return;                 /* 0 = "-- Custom --" */
    int count = 0;
    const Preset* list = binaural_beat_list(&count);
    if (sel - 1 >= count) return;
    gs->beat_hz = list[sel - 1].hz;
    char b[32]; snprintf(b, sizeof b, "%.3f", gs->beat_hz);
    SetWindowTextA(gs->hBeatEdit, b);
    if (!gs->advanced) { recompute_lr_from_base_beat(gs); update_lr_display(gs); }
    push_params(gs);
}
```

- [ ] **Step 7: Add a combo-population helper**

Insert after `populate_presets()` (after line 141):

```c
static void populate_binaural_combos(GuiState* gs) {
    SendMessageA(gs->hCarrierCombo, CB_ADDSTRING, 0, (LPARAM)"-- Custom --");
    int cc = 0; const Preset* cl = binaural_carrier_list(&cc);
    for (int i = 0; i < cc; i++)
        SendMessageA(gs->hCarrierCombo, CB_ADDSTRING, 0, (LPARAM)cl[i].name);
    SendMessageA(gs->hBeatCombo, CB_ADDSTRING, 0, (LPARAM)"-- Custom --");
    int bc = 0; const Preset* bl = binaural_beat_list(&bc);
    for (int i = 0; i < bc; i++)
        SendMessageA(gs->hBeatCombo, CB_ADDSTRING, 0, (LPARAM)bl[i].name);
}
```

(ASCII `-- Custom --`, not an em-dash: the controls use the ANSI `...A` API.)

- [ ] **Step 8: Replace the combo creation + reflow the Mode group**

Replace the entire "Binaural presets combo" block (lines ~361-373, from the `/* Binaural presets combo */` comment through the closing `}` of the population loop) with:

```c
    /* Binaural carrier preset */
    CreateWindowA("STATIC", "Carrier:", WS_CHILD | WS_VISIBLE,
                  25, 142, 60, 18, gs->hwnd, NULL, inst, NULL);
    gs->hCarrierCombo = CreateWindowA("COMBOBOX", NULL,
                  WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST | WS_VSCROLL,
                  90, 139, 320, 200, gs->hwnd,
                  (HMENU)(LONG_PTR)ID_CARRIER_COMBO, inst, NULL);

    /* Binaural beat preset */
    CreateWindowA("STATIC", "Beat:", WS_CHILD | WS_VISIBLE,
                  25, 170, 60, 18, gs->hwnd, NULL, inst, NULL);
    gs->hBeatCombo = CreateWindowA("COMBOBOX", NULL,
                  WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST | WS_VSCROLL,
                  90, 167, 320, 200, gs->hwnd,
                  (HMENU)(LONG_PTR)ID_BEAT_COMBO, inst, NULL);
```

- [ ] **Step 9: Shift the Base/Beat, Advanced, and L/R rows down by 28px**

Replace the Base/Beat block (lines ~375-388) with the same controls at the new y-coordinates:

```c
    CreateWindowA("STATIC", "Base:", WS_CHILD | WS_VISIBLE,
                  25, 198, 35, 18, gs->hwnd, NULL, inst, NULL);
    gs->hBaseEdit = CreateWindowA("EDIT", "200.000",
                  WS_CHILD | WS_VISIBLE | WS_BORDER | ES_AUTOHSCROLL,
                  65, 195, 80, 22, gs->hwnd,
                  (HMENU)(LONG_PTR)ID_BASE_EDIT, inst, NULL);
    CreateWindowA("STATIC", "Hz   Beat:", WS_CHILD | WS_VISIBLE,
                  150, 198, 70, 18, gs->hwnd, NULL, inst, NULL);
    gs->hBeatEdit = CreateWindowA("EDIT", "7.830",
                  WS_CHILD | WS_VISIBLE | WS_BORDER | ES_AUTOHSCROLL,
                  225, 195, 80, 22, gs->hwnd,
                  (HMENU)(LONG_PTR)ID_BEAT_EDIT, inst, NULL);
    CreateWindowA("STATIC", "Hz", WS_CHILD | WS_VISIBLE,
                  310, 198, 25, 18, gs->hwnd, NULL, inst, NULL);
```

Replace the Advanced checkbox (lines ~390-393) with:

```c
    gs->hAdvCheck = CreateWindowA("BUTTON", "Advanced (set L/R directly)",
                  WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX,
                  25, 223, 250, 20, gs->hwnd,
                  (HMENU)(LONG_PTR)ID_ADV_CHECK, inst, NULL);
```

Replace the L/R block (lines ~395-408) with:

```c
    CreateWindowA("STATIC", "L:", WS_CHILD | WS_VISIBLE,
                  25, 250, 15, 18, gs->hwnd, NULL, inst, NULL);
    gs->hLeftEdit = CreateWindowA("EDIT", "200.000",
                  WS_CHILD | WS_VISIBLE | WS_BORDER | ES_AUTOHSCROLL,
                  45, 247, 80, 22, gs->hwnd,
                  (HMENU)(LONG_PTR)ID_LEFT_EDIT, inst, NULL);
    CreateWindowA("STATIC", "Hz   R:", WS_CHILD | WS_VISIBLE,
                  130, 250, 50, 18, gs->hwnd, NULL, inst, NULL);
    gs->hRightEdit = CreateWindowA("EDIT", "207.830",
                  WS_CHILD | WS_VISIBLE | WS_BORDER | ES_AUTOHSCROLL,
                  185, 247, 80, 22, gs->hwnd,
                  (HMENU)(LONG_PTR)ID_RIGHT_EDIT, inst, NULL);
    CreateWindowA("STATIC", "Hz", WS_CHILD | WS_VISIBLE,
                  270, 250, 25, 18, gs->hwnd, NULL, inst, NULL);
```

- [ ] **Step 10: Grow the Mode group box**

Replace the Mode group box creation (line ~332-333):

```c
    CreateWindowA("BUTTON", "Mode", WS_CHILD | WS_VISIBLE | BS_GROUPBOX,
                  10, 95, 420, 160, gs->hwnd, NULL, inst, NULL);
```

with (height 160 → 188):

```c
    CreateWindowA("BUTTON", "Mode", WS_CHILD | WS_VISIBLE | BS_GROUPBOX,
                  10, 95, 420, 188, gs->hwnd, NULL, inst, NULL);
```

- [ ] **Step 11: Shift the Output group, its contents, and the Brain Cleaner group down by 28px**

Apply these exact coordinate changes (only the y-values change unless noted):

- Output group box: `10, 265, 420, 122` → `10, 293, 420, 122`
- "Volume:" static: `25, 288, ...` → `25, 316, ...`
- `hVolumeTrack`: `80, 285, ...` → `80, 313, ...`
- `hVolumeLabel`: `310, 288, ...` → `310, 316, ...`
- Hearing-warning static: `25, 312, ...` → `25, 340, ...`
- `hPlayBtn`: `80, 333, ...` → `80, 361, ...`
- `hStopBtn`: `185, 333, ...` → `185, 361, ...`
- `hStatusLabel`: `25, 366, ...` → `25, 394, ...`
- Brain Cleaner group box: `10, 393, 420, 85` → `10, 421, 420, 85`
- `hBrainStartBtn`: `80, 416, ...` → `80, 444, ...`
- `hBrainStopBtn`: `205, 416, ...` → `205, 444, ...`
- `hBrainTimerLabel`: `25, 450, ...` → `25, 478, ...`

- [ ] **Step 12: Grow the window and update defaults**

Replace the window creation size (line ~303), changing height `530` → `558`:

```c
    gs->hwnd = CreateWindowA(WIN_CLASS, "Tone Generator", style,
                             CW_USEDEFAULT, CW_USEDEFAULT, 460, 558,
                             NULL, NULL, inst, gs);
```

Update the startup defaults in `gui_create()` (lines ~282-285), Base 100 → 200:

```c
    gs->base_hz    = 200.0;
    gs->beat_hz    = 7.83;
    gs->left_hz    = 200.0;
    gs->right_hz   = 207.83;
```

- [ ] **Step 13: Populate + sync the new combos at startup**

In `gui_create()`, find the `populate_presets(gs);` call (line ~463) and replace it with:

```c
    populate_presets(gs);
    populate_binaural_combos(gs);
    sync_binaural_combos(gs);
```

- [ ] **Step 14: Update the WM_COMMAND dispatch**

In `wnd_proc`, replace the `ID_BINAURAL_COMBO` case (line ~516):

```c
                case ID_BINAURAL_COMBO: if (code == CBN_SELCHANGE) on_binaural_preset_changed(gs); break;
```

with:

```c
                case ID_CARRIER_COMBO: if (code == CBN_SELCHANGE) on_carrier_preset_changed(gs); break;
                case ID_BEAT_COMBO:    if (code == CBN_SELCHANGE) on_beat_preset_changed(gs); break;
```

- [ ] **Step 15: Build**

Run: `make`
Expected: `ToneGen.exe` builds with no warnings or errors. (If GCC reports `hBinauralCombo` / `ID_BINAURAL_COMBO` / `on_binaural_preset_changed` still referenced, a replacement above was missed — fix before continuing.)

- [ ] **Step 16: Manual smoke test**

Run: `./ToneGen.exe` and verify:
1. Window opens fully (Brain Cleaner group and its timer label are not clipped at the bottom).
2. Click **Binaural**. The **Carrier** and **Beat** dropdowns enable; Carrier shows `200 (neutral)`, Beat shows `Schumann 7.83 Hz`; Base=`200.000`, Beat=`7.830`, L=`200.000`, R=`207.830`.
3. Pick Carrier → `Solfeggio MI 528`. Base becomes `528.000`, L=`528.000`, R=`535.830`.
4. Pick Beat → `Gamma 40 Hz`. Beat becomes `40.000`, R=`568.000`.
5. Hand-edit Base to `530`, press Enter/Tab. Carrier dropdown flips to `-- Custom --`; Beat dropdown still shows `Gamma 40 Hz`.
6. Check **Advanced (set L/R directly)**. Both dropdowns (and Base/Beat fields) disable; L/R fields enable.
7. Press **Play** — audio is audible; **Stop** silences it.

- [ ] **Step 17: Commit**

```bash
git add src/gui.c
git commit -m "gui: split binaural preset into Carrier and Beat dropdowns with smart sync"
```

---

## Task 3: Remove the dead `BinauralPreset` API

**Files:**
- Modify: `src/presets.h`, `src/presets.c`

- [ ] **Step 1: Confirm nothing still references the old API**

Run: `grep -rn "BinauralPreset\|binaural_presets_list" src tests`
Expected: matches ONLY in `src/presets.h` and `src/presets.c` (no `gui.c`, no tests). If `gui.c` appears, Task 2 is incomplete — stop and fix.

- [ ] **Step 2: Remove from `src/presets.h`**

Delete these lines (the old struct + accessor, lines ~13-19):

```c
typedef struct {
    const char* name;
    double      base_hz;
    double      beat_hz;
} BinauralPreset;

const BinauralPreset* binaural_presets_list(int* out_count);
```

- [ ] **Step 3: Remove from `src/presets.c`**

Delete the entire `BINAURAL_PRESETS[]` array definition and the `binaural_presets_list()` function (the original lines ~26-53).

- [ ] **Step 4: Build and test**

Run: `make && make test`
Expected: `ToneGen.exe` builds clean; `test_synth` and `test_presets` both pass.

- [ ] **Step 5: Commit**

```bash
git add src/presets.h src/presets.c
git commit -m "presets: remove obsolete BinauralPreset (solfeggio-as-beat) API"
```

---

## Task 4: Documentation

**Files:**
- Modify: `README.md`, `ReadMe.txt`

- [ ] **Step 1: Fix the README dropdown description**

In `README.md`, replace line 33:

```
A **Binaural preset** dropdown provides common brainwave states (Delta 2 Hz, Theta 6 Hz, Schumann 7.83 Hz, Alpha 10 Hz, SMR 14 Hz, Beta 20 Hz, Gamma 40 Hz) plus all Solfeggio and tuning frequencies -- these automatically set the Base and Beat fields for you.
```

with:

```
Two dropdowns let you compose a session independently. The **Carrier** dropdown sets the pitch you consciously hear -- a neutral 200 Hz, the Solfeggio frequencies, or concert tunings (Middle C, A432, A440). The **Beat** dropdown sets the brainwave target your mind entrains to: Delta 2 Hz, Theta 6 Hz, Schumann 7.83 Hz, Alpha 10 Hz, SMR 14 Hz, Beta 20 Hz, Gamma 40 Hz. Each picks into the Base and Beat fields, which you can still edit by hand; a field that matches no preset shows `-- Custom --`.
```

- [ ] **Step 2: Fix the ReadMe.txt dropdown description**

In `ReadMe.txt`, replace the passage at lines ~38-41:

```
    the effect. A dropdown of binaural presets is available with common
    brainwave states (Delta, Theta, Schumann, Alpha, SMR, Beta, Gamma)
    plus all Solfeggio and tuning frequencies -- these automatically
    set the Base and Beat fields for you.
```

with:

```
    the effect. Two dropdowns let you compose a session: a Carrier
    dropdown (the pitch you hear -- neutral 200 Hz, the Solfeggio
    frequencies, or concert tunings) and a Beat dropdown (the
    brainwave target: Delta, Theta, Schumann, Alpha, SMR, Beta,
    Gamma). Each picks into the Base and Beat fields, which you can
    still edit by hand.
```

- [ ] **Step 3: Sanity-check the docs**

Run: `grep -n -i "carrier" README.md ReadMe.txt`
Expected: the new Carrier descriptions appear in both files.

- [ ] **Step 4: Commit**

```bash
git add README.md ReadMe.txt
git commit -m "docs: describe the new Carrier/Beat binaural dropdowns"
```

---

## Self-Review Notes

- **Spec coverage:** data model (Task 1), two combos + smart sync + layout + defaults (Task 2), old-API removal (Task 3), docs + invariant tests (Tasks 1 & 4). All spec sections map to a task.
- **Naming consistency:** `binaural_carrier_list` / `binaural_beat_list` / `presets_match_index` / `sync_binaural_combos` / `on_carrier_preset_changed` / `on_beat_preset_changed` are used identically across declaration, definition, and call sites.
- **Doc-fix correction vs spec:** the spec mentioned "fix the line-118 guidance," but that section concerns genuinely sub-audible (<20 Hz) frequencies and is correct as written; the actual fix is the dropdown description (README line 33 / ReadMe.txt line 38), which is what Task 4 targets.
- **Custom label:** spec wrote `— Custom —` (em-dash); the plan uses ASCII `-- Custom --` because the GUI uses the ANSI `CreateWindowA` / `SendMessageA` path.
