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

static const BinauralPreset BINAURAL_PRESETS[] = {
    { "Delta - Deep sleep (2 Hz)",        200.0,  2.0  },
    { "Theta - Meditation (6 Hz)",        200.0,  6.0  },
    { "Schumann - Earth (7.83 Hz)",       200.0,  7.83 },
    { "Alpha - Relaxation (10 Hz)",       200.0, 10.0  },
    { "SMR - Calm focus (14 Hz)",         200.0, 14.0  },
    { "Beta - Concentration (20 Hz)",     200.0, 20.0  },
    { "Gamma - Insight (40 Hz)",          200.0, 40.0  },
};

const BinauralPreset* binaural_presets_list(int* out_count) {
    *out_count = (int)(sizeof BINAURAL_PRESETS / sizeof BINAURAL_PRESETS[0]);
    return BINAURAL_PRESETS;
}
