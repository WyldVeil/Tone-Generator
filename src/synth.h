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

/* Fills `out` with `frames` stereo (L,R) interleaved int16 samples.
 * `out` must point to space for at least 2*frames int16_t values.
 * Updates `*phase` in place. Returns the final gain value reached. */
double synth_fill_buffer(int16_t* out, size_t frames,
                         SynthPhase* phase,
                         SynthFrameParams p);

#endif
