#include "gui.h"
#include "presets.h"

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

#define WIN_CLASS         "ToneGenMain"

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
                             CW_USEDEFAULT, CW_USEDEFAULT, 416, 390,
                             NULL, NULL, inst, gs);
    if (!gs->hwnd) { free(gs); return NULL; }
    /* USERDATA is also installed inside WM_NCCREATE for any messages
     * that fire synchronously during CreateWindowA; redundant here is
     * cheap and keeps the post-return state explicit. */
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
        case WM_HSCROLL: {
            if (gs && (HWND)lp == gs->hVolumeTrack) on_volume_changed(gs);
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
