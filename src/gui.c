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
