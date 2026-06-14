#include "presets.h"
#include <stddef.h>

static const Preset PRESETS[] = {
    { "Schumann resonance",        7.83 },
    { "Gamma / insight (40 Hz)",  40.00 },
    { "Middle C",                261.63 },
    { "Concert A (Verdi 432)",   432.00 },
    { "Concert A (standard)",    440.00 },
    { "Solfeggio 174",           174.00 },
    { "Solfeggio 285",           285.00 },
    { "Solfeggio UT (396)",      396.00 },
    { "Solfeggio RE (417)",      417.00 },
    { "Solfeggio MI (528)",      528.00 },
    { "Solfeggio FA (639)",      639.00 },
    { "Solfeggio SOL (741)",     741.00 },
    { "Solfeggio LA (852)",      852.00 },
    { "Solfeggio 963",           963.00 },
};

const Preset* presets_list(int* out_count) {
    *out_count = (int)(sizeof PRESETS / sizeof PRESETS[0]);
    return PRESETS;
}

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
