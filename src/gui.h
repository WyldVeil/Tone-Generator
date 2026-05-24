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
