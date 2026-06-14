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

    printf("test_presets: %d presets, 3 named lookups passed\n", count);
    return 0;
}
