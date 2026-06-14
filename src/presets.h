#ifndef PRESETS_H
#define PRESETS_H

typedef struct {
    const char* name;
    double      hz;
} Preset;

/* Returns a pointer to a static array of presets.
 * Writes the count to *out_count. Never returns NULL. */
const Preset* presets_list(int* out_count);

/* Binaural-mode carrier presets (the audible pitch sent to both ears). */
const Preset* binaural_carrier_list(int* out_count);

/* Binaural-mode beat presets (the |R-L| difference the brain entrains to). */
const Preset* binaural_beat_list(int* out_count);

/* Returns the index into `list` whose hz is within `tol` of `hz`,
 * or -1 if none matches. First match wins. */
int presets_match_index(const Preset* list, int count, double hz, double tol);

#endif
