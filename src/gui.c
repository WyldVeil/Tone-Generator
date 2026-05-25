#include "gui.h"
#include "presets.h"
#include "synth.h"

#include <commctrl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define ID_PRESET_COMBO   1001
#define ID_CUSTOM_EDIT    1002
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
#define ID_MODE_NOISE     1013
#define ID_NOISE_TYPE     1014
#define ID_BINAURAL_COMBO 1015
#define ID_BRAIN_START    1016
#define ID_BRAIN_STOP     1017
#define ID_BRAIN_TIMER    2001
#define BRAIN_SESSION_SEC 3600

#define WIN_CLASS         "ToneGenMain"

struct GuiState {
    HINSTANCE   inst;
    HWND        hwnd;
    AudioState* audio;

    HWND hPresetCombo;
    HWND hCustomEdit;
    HWND hModeSingle, hModeBinaural, hModeNoise;
    HWND hBaseEdit, hBeatEdit;
    HWND hAdvCheck;
    HWND hLeftEdit, hRightEdit;
    HWND hBinauralCombo;
    HWND hNoiseTypeCombo;
    HWND hVolumeTrack, hVolumeLabel;
    HWND hPlayBtn, hStopBtn;
    HWND hStatusLabel;
    HWND hBrainStartBtn, hBrainStopBtn;
    HWND hBrainTimerLabel;

    double custom_hz;
    double base_hz, beat_hz;
    double left_hz, right_hz;
    int    binaural;     /* 0 = single, 1 = binaural */
    int    advanced;     /* 0 = base+beat, 1 = L/R direct */
    int    noise_mode;   /* 0 = tonal, 1 = noise */
    int    noise_type;   /* NOISE_WHITE / NOISE_PINK / NOISE_BROWN */
    int    volume_pct;   /* 0..100 */
    int    brain_cleaner;
    int    brain_seconds_left;
};

static LRESULT CALLBACK wnd_proc(HWND, UINT, WPARAM, LPARAM);

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
    int bc = gs->brain_cleaner;
    int nm = gs->noise_mode;
    int bn = gs->binaural;
    EnableWindow(gs->hPresetCombo,  !bc && !nm && !bn);
    EnableWindow(gs->hCustomEdit,   !bc && !nm && !bn);
    EnableWindow(gs->hModeSingle,   !bc);
    EnableWindow(gs->hModeBinaural, !bc);
    if (gs->hModeNoise)      EnableWindow(gs->hModeNoise,      !bc);
    if (gs->hBinauralCombo)  EnableWindow(gs->hBinauralCombo,  !bc && bn);
    EnableWindow(gs->hBaseEdit,     !bc && !nm && bn && !gs->advanced);
    EnableWindow(gs->hBeatEdit,     !bc && !nm && bn && !gs->advanced);
    EnableWindow(gs->hLeftEdit,     !bc && !nm && bn &&  gs->advanced);
    EnableWindow(gs->hRightEdit,    !bc && !nm && bn &&  gs->advanced);
    EnableWindow(gs->hAdvCheck,     !bc && !nm && bn);
    if (gs->hNoiseTypeCombo) EnableWindow(gs->hNoiseTypeCombo, !bc && nm);
    EnableWindow(gs->hPlayBtn,      !bc);
    EnableWindow(gs->hStopBtn,      !bc);
    if (gs->hBrainStartBtn)  EnableWindow(gs->hBrainStartBtn, !bc);
    if (gs->hBrainStopBtn)   EnableWindow(gs->hBrainStopBtn,   bc);
}

static void update_status(GuiState* gs) {
    const char* base = audio_is_playing(gs->audio) ? "Playing" : "Idle";
    char msg[128];
    if (gs->brain_cleaner) {
        int m = gs->brain_seconds_left / 60;
        int s = gs->brain_seconds_left % 60;
        snprintf(msg, sizeof msg,
                 "Status: %s  (40 Hz Brain Cleaner - %d:%02d left)", base, m, s);
    } else if (!gs->binaural && (gs->custom_hz < 20.0 || gs->custom_hz > 20000.0)) {
        snprintf(msg, sizeof msg,
                 "Status: %s  (note: %.2f Hz is outside audible range)",
                 base, gs->custom_hz);
    } else {
        snprintf(msg, sizeof msg, "Status: %s", base);
    }
    SetWindowTextA(gs->hStatusLabel, msg);
}

static void push_params(GuiState* gs) {
    AudioParams p = {0};
    if (gs->brain_cleaner) {
        p.left_hz  = 10000.0;
        p.right_hz = 10000.0;
        p.mod_hz   = 40.0;
    } else if (gs->noise_mode) {
        p.noise_type = gs->noise_type;
    } else if (gs->binaural) {
        p.left_hz  = gs->left_hz;
        p.right_hz = gs->right_hz;
    } else {
        p.left_hz = p.right_hz = gs->custom_hz;
    }
    p.volume = (double)gs->volume_pct / 100.0;
    audio_set_params(gs->audio, p);
    update_status(gs);
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
    double hz = list[sel].hz;
    if (gs->binaural) {
        gs->beat_hz = hz;
        char b[32]; snprintf(b, sizeof b, "%.3f", hz);
        SetWindowTextA(gs->hBeatEdit, b);
        if (!gs->advanced) { recompute_lr_from_base_beat(gs); update_lr_display(gs); }
    } else {
        gs->custom_hz = hz;
        char b[32]; snprintf(b, sizeof b, "%.3f", hz);
        SetWindowTextA(gs->hCustomEdit, b);
    }
    push_params(gs);
}

static void on_custom_committed(GuiState* gs) {
    char buf[32] = {0};
    GetWindowTextA(gs->hCustomEdit, buf, sizeof buf);
    char* end = NULL;
    double v = strtod(buf, &end);
    int valid = (end != buf) && (v >= 0.001) && (v <= 22050.0);
    if (!valid) {
        /* Revert display to last good value; don't push to audio. */
        char fixed[32];
        snprintf(fixed, sizeof fixed, "%.3f", gs->custom_hz);
        SetWindowTextA(gs->hCustomEdit, fixed);
        return;
    }
    gs->custom_hz = v;
    char fixed[32];
    snprintf(fixed, sizeof fixed, "%.3f", v);
    SetWindowTextA(gs->hCustomEdit, fixed);
    SendMessageA(gs->hPresetCombo, CB_SETCURSEL, (WPARAM)-1, 0);
    push_params(gs);
}

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

static void on_mode_changed(GuiState* gs, int binaural, int noise) {
    gs->binaural = binaural;
    gs->noise_mode = noise;
    if (noise && gs->noise_type == 0) gs->noise_type = NOISE_WHITE;
    apply_enabled_states(gs);
    push_params(gs);
}

static void on_noise_type_changed(GuiState* gs) {
    int sel = (int)SendMessageA(gs->hNoiseTypeCombo, CB_GETCURSEL, 0, 0);
    if (sel == 0) gs->noise_type = NOISE_WHITE;
    else if (sel == 1) gs->noise_type = NOISE_PINK;
    else if (sel == 2) gs->noise_type = NOISE_BROWN;
    push_params(gs);
}

static void on_binaural_preset_changed(GuiState* gs) {
    int sel = (int)SendMessageA(gs->hBinauralCombo, CB_GETCURSEL, 0, 0);
    if (sel < 0) return;
    int count = 0;
    const BinauralPreset* list = binaural_presets_list(&count);
    if (sel >= count) return;
    gs->base_hz = list[sel].base_hz;
    gs->beat_hz = list[sel].beat_hz;
    char b[32];
    snprintf(b, sizeof b, "%.3f", gs->base_hz); SetWindowTextA(gs->hBaseEdit, b);
    snprintf(b, sizeof b, "%.3f", gs->beat_hz); SetWindowTextA(gs->hBeatEdit, b);
    if (!gs->advanced) { recompute_lr_from_base_beat(gs); update_lr_display(gs); }
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

GuiState* gui_create(HINSTANCE inst, AudioState* audio) {
    GuiState* gs = (GuiState*)calloc(1, sizeof *gs);
    if (!gs) return NULL;
    gs->inst = inst;
    gs->audio = audio;
    gs->custom_hz = 440.0;
    gs->base_hz    = 100.0;
    gs->beat_hz    = 7.83;
    gs->left_hz    = 100.0;
    gs->right_hz   = 107.83;
    gs->binaural   = 0;
    gs->advanced   = 0;
    gs->volume_pct = 50;

    INITCOMMONCONTROLSEX icc = { sizeof icc, ICC_BAR_CLASSES | ICC_STANDARD_CLASSES };
    InitCommonControlsEx(&icc);

    WNDCLASSA wc = {0};
    wc.lpfnWndProc   = wnd_proc;
    wc.hInstance     = inst;
    wc.hCursor       = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_BTNFACE + 1);
    wc.lpszClassName = WIN_CLASS;
    if (!RegisterClassA(&wc)) { free(gs); return NULL; }

    DWORD style = WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX;
    gs->hwnd = CreateWindowA(WIN_CLASS, "Tone Generator", style,
                             CW_USEDEFAULT, CW_USEDEFAULT, 460, 530,
                             NULL, NULL, inst, gs);
    if (!gs->hwnd) { free(gs); return NULL; }
    /* USERDATA is also installed inside WM_NCCREATE for any messages
     * that fire synchronously during CreateWindowA; redundant here is
     * cheap and keeps the post-return state explicit. */
    SetWindowLongPtrA(gs->hwnd, GWLP_USERDATA, (LONG_PTR)gs);

    /* Frequency group box at top */
    CreateWindowA("BUTTON", "Frequency", WS_CHILD | WS_VISIBLE | BS_GROUPBOX,
                  10, 5, 420, 80, gs->hwnd, NULL, inst, NULL);

    CreateWindowA("STATIC", "Preset:", WS_CHILD | WS_VISIBLE,
                  25, 28, 50, 18, gs->hwnd, NULL, inst, NULL);
    gs->hPresetCombo = CreateWindowA("COMBOBOX", NULL,
                  WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST | WS_VSCROLL,
                  80, 25, 330, 200, gs->hwnd,
                  (HMENU)(LONG_PTR)ID_PRESET_COMBO, inst, NULL);

    CreateWindowA("STATIC", "Custom:", WS_CHILD | WS_VISIBLE,
                  25, 58, 50, 18, gs->hwnd, NULL, inst, NULL);
    gs->hCustomEdit = CreateWindowA("EDIT", "440.000",
                  WS_CHILD | WS_VISIBLE | WS_BORDER | ES_AUTOHSCROLL,
                  80, 55, 100, 22, gs->hwnd,
                  (HMENU)(LONG_PTR)ID_CUSTOM_EDIT, inst, NULL);
    CreateWindowA("STATIC", "Hz", WS_CHILD | WS_VISIBLE,
                  185, 58, 30, 18, gs->hwnd, NULL, inst, NULL);

    /* Mode group */
    CreateWindowA("BUTTON", "Mode", WS_CHILD | WS_VISIBLE | BS_GROUPBOX,
                  10, 95, 420, 160, gs->hwnd, NULL, inst, NULL);

    gs->hModeSingle = CreateWindowA("BUTTON", "Single tone",
                  WS_CHILD | WS_VISIBLE | BS_AUTORADIOBUTTON | WS_GROUP,
                  25, 115, 90, 20, gs->hwnd,
                  (HMENU)(LONG_PTR)ID_MODE_SINGLE, inst, NULL);
    gs->hModeBinaural = CreateWindowA("BUTTON", "Binaural",
                  WS_CHILD | WS_VISIBLE | BS_AUTORADIOBUTTON,
                  120, 115, 75, 20, gs->hwnd,
                  (HMENU)(LONG_PTR)ID_MODE_BINAURAL, inst, NULL);
    gs->hModeNoise = CreateWindowA("BUTTON", "Noise",
                  WS_CHILD | WS_VISIBLE | BS_AUTORADIOBUTTON,
                  200, 115, 60, 20, gs->hwnd,
                  (HMENU)(LONG_PTR)ID_MODE_NOISE, inst, NULL);
    SendMessageA(gs->hModeSingle, BM_SETCHECK, BST_CHECKED, 0);

    /* Noise type combo (visible in noise mode) */
    CreateWindowA("STATIC", "Type:", WS_CHILD | WS_VISIBLE,
                  270, 118, 35, 18, gs->hwnd, NULL, inst, NULL);
    gs->hNoiseTypeCombo = CreateWindowA("COMBOBOX", NULL,
                  WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST,
                  305, 115, 75, 100, gs->hwnd,
                  (HMENU)(LONG_PTR)ID_NOISE_TYPE, inst, NULL);
    SendMessageA(gs->hNoiseTypeCombo, CB_ADDSTRING, 0, (LPARAM)"White");
    SendMessageA(gs->hNoiseTypeCombo, CB_ADDSTRING, 0, (LPARAM)"Pink");
    SendMessageA(gs->hNoiseTypeCombo, CB_ADDSTRING, 0, (LPARAM)"Brown");
    SendMessageA(gs->hNoiseTypeCombo, CB_SETCURSEL, 0, 0);

    /* Binaural presets combo */
    CreateWindowA("STATIC", "Binaural preset:", WS_CHILD | WS_VISIBLE,
                  25, 142, 95, 18, gs->hwnd, NULL, inst, NULL);
    gs->hBinauralCombo = CreateWindowA("COMBOBOX", NULL,
                  WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST | WS_VSCROLL,
                  125, 139, 285, 200, gs->hwnd,
                  (HMENU)(LONG_PTR)ID_BINAURAL_COMBO, inst, NULL);
    {
        int bcount = 0;
        const BinauralPreset* blist = binaural_presets_list(&bcount);
        for (int i = 0; i < bcount; i++)
            SendMessageA(gs->hBinauralCombo, CB_ADDSTRING, 0, (LPARAM)blist[i].name);
    }

    CreateWindowA("STATIC", "Base:", WS_CHILD | WS_VISIBLE,
                  25, 170, 35, 18, gs->hwnd, NULL, inst, NULL);
    gs->hBaseEdit = CreateWindowA("EDIT", "100.000",
                  WS_CHILD | WS_VISIBLE | WS_BORDER | ES_AUTOHSCROLL,
                  65, 167, 80, 22, gs->hwnd,
                  (HMENU)(LONG_PTR)ID_BASE_EDIT, inst, NULL);
    CreateWindowA("STATIC", "Hz   Beat:", WS_CHILD | WS_VISIBLE,
                  150, 170, 70, 18, gs->hwnd, NULL, inst, NULL);
    gs->hBeatEdit = CreateWindowA("EDIT", "7.830",
                  WS_CHILD | WS_VISIBLE | WS_BORDER | ES_AUTOHSCROLL,
                  225, 167, 80, 22, gs->hwnd,
                  (HMENU)(LONG_PTR)ID_BEAT_EDIT, inst, NULL);
    CreateWindowA("STATIC", "Hz", WS_CHILD | WS_VISIBLE,
                  310, 170, 25, 18, gs->hwnd, NULL, inst, NULL);

    gs->hAdvCheck = CreateWindowA("BUTTON", "Advanced (set L/R directly)",
                  WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX,
                  25, 195, 250, 20, gs->hwnd,
                  (HMENU)(LONG_PTR)ID_ADV_CHECK, inst, NULL);

    CreateWindowA("STATIC", "L:", WS_CHILD | WS_VISIBLE,
                  25, 222, 15, 18, gs->hwnd, NULL, inst, NULL);
    gs->hLeftEdit = CreateWindowA("EDIT", "100.000",
                  WS_CHILD | WS_VISIBLE | WS_BORDER | ES_AUTOHSCROLL,
                  45, 219, 80, 22, gs->hwnd,
                  (HMENU)(LONG_PTR)ID_LEFT_EDIT, inst, NULL);
    CreateWindowA("STATIC", "Hz   R:", WS_CHILD | WS_VISIBLE,
                  130, 222, 50, 18, gs->hwnd, NULL, inst, NULL);
    gs->hRightEdit = CreateWindowA("EDIT", "107.830",
                  WS_CHILD | WS_VISIBLE | WS_BORDER | ES_AUTOHSCROLL,
                  185, 219, 80, 22, gs->hwnd,
                  (HMENU)(LONG_PTR)ID_RIGHT_EDIT, inst, NULL);
    CreateWindowA("STATIC", "Hz", WS_CHILD | WS_VISIBLE,
                  270, 222, 25, 18, gs->hwnd, NULL, inst, NULL);

    /* Output group */
    CreateWindowA("BUTTON", "Output", WS_CHILD | WS_VISIBLE | BS_GROUPBOX,
                  10, 265, 420, 122, gs->hwnd, NULL, inst, NULL);

    CreateWindowA("STATIC", "Volume:", WS_CHILD | WS_VISIBLE,
                  25, 288, 50, 18, gs->hwnd, NULL, inst, NULL);
    gs->hVolumeTrack = CreateWindowA(TRACKBAR_CLASSA, NULL,
                  WS_CHILD | WS_VISIBLE | TBS_HORZ | TBS_NOTICKS,
                  80, 285, 220, 24, gs->hwnd,
                  (HMENU)(LONG_PTR)ID_VOLUME_TRACK, inst, NULL);
    SendMessageA(gs->hVolumeTrack, TBM_SETRANGE, TRUE, MAKELONG(0, 100));
    SendMessageA(gs->hVolumeTrack, TBM_SETPOS,   TRUE, gs->volume_pct);
    gs->hVolumeLabel = CreateWindowA("STATIC", "50 %",
                  WS_CHILD | WS_VISIBLE,
                  310, 288, 60, 18, gs->hwnd, NULL, inst, NULL);

    CreateWindowA("STATIC",
                  "High volume may damage hearing. Use at your own risk.",
                  WS_CHILD | WS_VISIBLE | SS_OWNERDRAW,
                  25, 312, 400, 18, gs->hwnd, (HMENU)(LONG_PTR)9999, inst, NULL);

    gs->hPlayBtn = CreateWindowA("BUTTON", "Play",
                  WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                  80, 333, 90, 26, gs->hwnd,
                  (HMENU)(LONG_PTR)ID_PLAY_BUTTON, inst, NULL);
    gs->hStopBtn = CreateWindowA("BUTTON", "Stop",
                  WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                  185, 333, 90, 26, gs->hwnd,
                  (HMENU)(LONG_PTR)ID_STOP_BUTTON, inst, NULL);

    gs->hStatusLabel = CreateWindowA("STATIC", "Status: Idle",
                  WS_CHILD | WS_VISIBLE,
                  25, 366, 400, 18, gs->hwnd, NULL, inst, NULL);

    /* 40Hz Brain Cleaner group */
    CreateWindowA("BUTTON", "40Hz Brain Cleaner",
                  WS_CHILD | WS_VISIBLE | BS_GROUPBOX,
                  10, 393, 420, 85, gs->hwnd, NULL, inst, NULL);

    gs->hBrainStartBtn = CreateWindowA("BUTTON", "Start (60 min)",
                  WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                  80, 416, 110, 26, gs->hwnd,
                  (HMENU)(LONG_PTR)ID_BRAIN_START, inst, NULL);
    gs->hBrainStopBtn = CreateWindowA("BUTTON", "Stop",
                  WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                  205, 416, 90, 26, gs->hwnd,
                  (HMENU)(LONG_PTR)ID_BRAIN_STOP, inst, NULL);
    gs->hBrainTimerLabel = CreateWindowA("STATIC", "60:00 session (MIT GENUS protocol)",
                  WS_CHILD | WS_VISIBLE,
                  25, 450, 400, 18, gs->hwnd, NULL, inst, NULL);

    apply_enabled_states(gs);

    populate_presets(gs);
    push_params(gs);
    return gs;
}

HWND gui_hwnd(GuiState* gs) { return gs ? gs->hwnd : NULL; }

int gui_run_message_loop(GuiState* gs) {
    MSG msg;
    while (GetMessageA(&msg, NULL, 0, 0) > 0) {
        if (msg.message == WM_KEYDOWN && msg.wParam == VK_RETURN) {
            char cls[16] = {0};
            GetClassNameA(msg.hwnd, cls, sizeof cls);
            if (strcmp(cls, "Edit") == 0) {
                SetFocus(gs->hwnd);
                continue;
            }
        }
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
    if (msg == WM_NCCREATE) {
        CREATESTRUCT* cs = (CREATESTRUCT*)lp;
        SetWindowLongPtrA(hwnd, GWLP_USERDATA, (LONG_PTR)cs->lpCreateParams);
        return DefWindowProcA(hwnd, msg, wp, lp);
    }
    GuiState* gs = (GuiState*)GetWindowLongPtrA(hwnd, GWLP_USERDATA);
    switch (msg) {
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
                case ID_MODE_SINGLE:    if (code == BN_CLICKED)    on_mode_changed(gs, 0, 0); break;
                case ID_MODE_BINAURAL:  if (code == BN_CLICKED)    on_mode_changed(gs, 1, 0); break;
                case ID_MODE_NOISE:     if (code == BN_CLICKED)    on_mode_changed(gs, 0, 1); break;
                case ID_NOISE_TYPE:     if (code == CBN_SELCHANGE) on_noise_type_changed(gs); break;
                case ID_BINAURAL_COMBO: if (code == CBN_SELCHANGE) on_binaural_preset_changed(gs); break;
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
                case ID_BRAIN_START:    if (code == BN_CLICKED) {
                    gs->brain_cleaner = 1;
                    gs->brain_seconds_left = BRAIN_SESSION_SEC;
                    apply_enabled_states(gs);
                    push_params(gs);
                    audio_play(gs->audio);
                    SetTimer(hwnd, ID_BRAIN_TIMER, 1000, NULL);
                    update_status(gs);
                } break;
                case ID_BRAIN_STOP:     if (code == BN_CLICKED) {
                    KillTimer(hwnd, ID_BRAIN_TIMER);
                    gs->brain_cleaner = 0;
                    audio_stop(gs->audio);
                    apply_enabled_states(gs);
                    push_params(gs);
                } break;
            }
            return 0;
        }
        case WM_DRAWITEM: {
            if (!gs) break;
            DRAWITEMSTRUCT* dis = (DRAWITEMSTRUCT*)lp;
            if (dis->CtlID == 9999) {
                SetBkMode(dis->hDC, TRANSPARENT);
                SetTextColor(dis->hDC, RGB(200, 0, 0));
                char txt[128] = {0};
                GetWindowTextA(dis->hwndItem, txt, sizeof txt);
                DrawTextA(dis->hDC, txt, -1, &dis->rcItem,
                          DT_LEFT | DT_VCENTER | DT_SINGLELINE);
                return TRUE;
            }
            break;
        }
        case WM_HSCROLL: {
            if (gs && (HWND)lp == gs->hVolumeTrack) on_volume_changed(gs);
            return 0;
        }
        case WM_TIMER: {
            if (!gs || wp != ID_BRAIN_TIMER) break;
            gs->brain_seconds_left--;
            if (gs->brain_seconds_left <= 0) {
                KillTimer(hwnd, ID_BRAIN_TIMER);
                gs->brain_cleaner = 0;
                audio_stop(gs->audio);
                apply_enabled_states(gs);
                push_params(gs);
                SetWindowTextA(gs->hBrainTimerLabel, "Session complete");
            } else {
                int m = gs->brain_seconds_left / 60;
                int s = gs->brain_seconds_left % 60;
                char tb[32]; snprintf(tb, sizeof tb, "%d:%02d remaining", m, s);
                SetWindowTextA(gs->hBrainTimerLabel, tb);
            }
            update_status(gs);
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
