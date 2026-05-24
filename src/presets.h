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
