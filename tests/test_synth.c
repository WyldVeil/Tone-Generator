#include "synth.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>

static int approx(double a, double b) { return fabs(a - b) < 1e-9; }

static void test_next_gain_moves_up_toward_target(void) {
    assert(approx(synth_next_gain(0.0, 1.0, 0.25), 0.25));
}

static void test_next_gain_moves_down_toward_target(void) {
    assert(approx(synth_next_gain(1.0, 0.0, 0.25), 0.75));
}

static void test_next_gain_does_not_overshoot_going_up(void) {
    assert(approx(synth_next_gain(0.9, 1.0, 0.25), 1.0));
}

static void test_next_gain_does_not_overshoot_going_down(void) {
    assert(approx(synth_next_gain(0.1, 0.0, 0.25), 0.0));
}

static void test_next_gain_at_target_returns_target(void) {
    assert(approx(synth_next_gain(0.5, 0.5, 0.1), 0.5));
}

static void test_fill_buffer_zero_freq_is_silent(void) {
    float buf[200] = {0};
    SynthPhase phase = {0};
    SynthFrameParams p = { 0.0, 0.0, 1.0, 1.0, 1.0, 0.0, 0.0 };
    synth_fill_buffer(buf, 100, &phase, p);
    for (int i = 0; i < 200; i++) assert(buf[i] == 0.0f);
}

static void test_fill_buffer_440hz_produces_nonzero(void) {
    float buf[200] = {0};
    SynthPhase phase = {0};
    SynthFrameParams p = { 440.0, 440.0, 1.0, 1.0, 1.0, 0.0, 0.0 };
    synth_fill_buffer(buf, 100, &phase, p);
    int nonzero = 0;
    for (int i = 0; i < 200; i++) if (buf[i] != 0.0f) nonzero++;
    assert(nonzero > 100);  /* most samples should be nonzero */
}

static void test_fill_buffer_advances_phase(void) {
    float buf[200];
    SynthPhase phase = {0};
    SynthFrameParams p = { 1000.0, 1000.0, 1.0, 1.0, 1.0, 0.0, 0.0 };
    synth_fill_buffer(buf, 100, &phase, p);
    /* After 100 frames at 1000 Hz / 44100 Hz: phase_l should equal
     * (2π · 1000 / 44100) · 100, wrapped to [0, 2π). */
    double expected = (2.0 * 3.14159265358979323846 * 1000.0 / 44100.0) * 100.0;
    while (expected >= 2.0 * 3.14159265358979323846) expected -= 2.0 * 3.14159265358979323846;
    assert(fabs(phase.phase_l - expected) < 1e-6);
}

static void test_fill_buffer_applies_volume(void) {
    float loud[200], quiet[200];
    SynthPhase pl = {0}, pq = {0};
    SynthFrameParams pa_loud  = { 440.0, 440.0, 1.0, 1.0, 1.0, 0.0, 0.0 };
    SynthFrameParams pa_quiet = { 440.0, 440.0, 0.1, 1.0, 1.0, 0.0, 0.0 };
    synth_fill_buffer(loud,  100, &pl, pa_loud);
    synth_fill_buffer(quiet, 100, &pq, pa_quiet);
    /* peak of quiet buffer should be roughly 1/10 the peak of loud buffer */
    float peak_loud = 0.0f, peak_quiet = 0.0f;
    for (int i = 0; i < 200; i++) {
        if (fabsf(loud[i])  > peak_loud)  peak_loud  = fabsf(loud[i]);
        if (fabsf(quiet[i]) > peak_quiet) peak_quiet = fabsf(quiet[i]);
    }
    assert(peak_quiet > 0.0f);
    assert(peak_loud > peak_quiet * 5.0f);  /* loose bound */
}

static void test_fill_buffer_ramps_gain(void) {
    float buf[200];
    SynthPhase phase = {0};
    /* Fade from gain 0 → 1 over 100 frames, step = 0.01 */
    SynthFrameParams p = { 440.0, 440.0, 1.0, 0.0, 1.0, 0.01, 0.0 };
    double final_gain = synth_fill_buffer(buf, 100, &phase, p);
    assert(fabs(final_gain - 1.0) < 1e-9);
    /* First sample should be ≈ 0 (gain ≈ 0), later samples larger */
    assert(fabsf(buf[0]) < fabsf(buf[198]));
}

int main(void) {
    test_next_gain_moves_up_toward_target();
    test_next_gain_moves_down_toward_target();
    test_next_gain_does_not_overshoot_going_up();
    test_next_gain_does_not_overshoot_going_down();
    test_next_gain_at_target_returns_target();
    test_fill_buffer_zero_freq_is_silent();
    test_fill_buffer_440hz_produces_nonzero();
    test_fill_buffer_advances_phase();
    test_fill_buffer_applies_volume();
    test_fill_buffer_ramps_gain();
    printf("test_synth: 10 passed\n");
    return 0;
}
