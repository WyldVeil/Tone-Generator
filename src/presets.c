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

static const BinauralPreset BINAURAL_PRESETS[] = {
    /* Brainwave presets (beat = target brainwave) */
    { "Delta - Deep sleep (2 Hz)",        200.0,   2.0   },
    { "Theta - Meditation (6 Hz)",        200.0,   6.0   },
    { "Schumann - Earth (7.83 Hz)",       200.0,   7.83  },
    { "Alpha - Relaxation (10 Hz)",       200.0,  10.0   },
    { "SMR - Calm focus (14 Hz)",         200.0,  14.0   },
    { "Beta - Concentration (20 Hz)",     200.0,  20.0   },
    { "Gamma - Insight (40 Hz)",          200.0,  40.0   },
    /* Solfeggio & tuning as beat frequencies */
    { "Solfeggio 174 Hz",                200.0, 174.0   },
    { "Solfeggio 285 Hz",                200.0, 285.0   },
    { "Solfeggio UT 396 Hz",             200.0, 396.0   },
    { "Solfeggio RE 417 Hz",             200.0, 417.0   },
    { "Solfeggio MI 528 Hz",             200.0, 528.0   },
    { "Solfeggio FA 639 Hz",             200.0, 639.0   },
    { "Solfeggio SOL 741 Hz",            200.0, 741.0   },
    { "Solfeggio LA 852 Hz",             200.0, 852.0   },
    { "Solfeggio 963 Hz",                200.0, 963.0   },
    { "Middle C 261.63 Hz",              200.0, 261.63  },
    { "Concert A (Verdi) 432 Hz",        200.0, 432.0   },
    { "Concert A (standard) 440 Hz",     200.0, 440.0   },
};

const BinauralPreset* binaural_presets_list(int* out_count) {
    *out_count = (int)(sizeof BINAURAL_PRESETS / sizeof BINAURAL_PRESETS[0]);
    return BINAURAL_PRESETS;
}
