#ifndef SYNTH_H
#define SYNTH_H

#include <stdint.h>
#include <stddef.h>

/* Move current toward target by at most step. Never overshoot.
 * Returns the new gain value. Step must be positive. */
double synth_next_gain(double current, double target, double step);

#define SYNTH_SAMPLE_RATE 44100

/* Per-channel phase accumulator. Phases are in radians [0, 2π).
 * Hold these in the caller across calls so that frequency changes
 * don't cause phase discontinuities (clicks). */
typedef struct {
    double phase_l;
    double phase_r;
    double phase_mod;
} SynthPhase;

/* Parameters needed to render one buffer of audio.
 * `gain_start` is multiplied with each sample at frame 0 of the buffer,
 * then advances toward `gain_target` by `gain_step` per sample (per
 * synth_next_gain semantics). */
typedef struct {
    double left_hz;
    double right_hz;
    double volume;
    double gain_start;
    double gain_target;
    double gain_step;
    double mod_hz;
} SynthFrameParams;

/* Fills `out` with `frames` stereo (L,R) interleaved float samples in the
 * normalized range [-1.0, 1.0] (the native format of the WASAPI shared mixer).
 * `out` must point to space for at least 2*frames float values.
 * Updates `*phase` in place. Returns the final gain value reached. */
double synth_fill_buffer(float* out, size_t frames,
                         SynthPhase* phase,
                         SynthFrameParams p);

#define NOISE_NONE  0
#define NOISE_WHITE 1
#define NOISE_PINK  2
#define NOISE_BROWN 3

typedef struct {
    uint32_t seed;
    double b0, b1, b2, b3, b4, b5, b6;
    double brown;
} NoiseChannel;

typedef struct {
    NoiseChannel left;
    NoiseChannel right;
} NoiseState;

void noise_state_init(NoiseState* ns);

double synth_fill_noise(float* out, size_t frames, NoiseState* ns,
                        int noise_type, double volume,
                        double gain_start, double gain_target, double gain_step);

#endif
