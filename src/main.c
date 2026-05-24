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
