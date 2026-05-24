#include "synth.h"

double synth_next_gain(double current, double target, double step) {
    if (current < target) {
        double next = current + step;
        return next > target ? target : next;
    }
    if (current > target) {
        double next = current - step;
        return next < target ? target : next;
    }
    return current;
}

#include <math.h>

#define TWO_PI 6.28318530717958647692

double synth_fill_buffer(int16_t* out, size_t frames,
                         SynthPhase* phase,
                         SynthFrameParams p)
{
    /* Per-sample phase increments (radians per sample).
     * If hz is 0, the channel produces silence regardless of phase. */
    double inc_l = TWO_PI * p.left_hz  / (double)SYNTH_SAMPLE_RATE;
    double inc_r = TWO_PI * p.right_hz / (double)SYNTH_SAMPLE_RATE;

    double pl = phase->phase_l;
    double pr = phase->phase_r;
    double gain = p.gain_start;

    for (size_t i = 0; i < frames; i++) {
        double sl = sin(pl) * p.volume * gain;
        double sr = sin(pr) * p.volume * gain;
        out[2*i]     = (int16_t)(sl * 32767.0);
        out[2*i + 1] = (int16_t)(sr * 32767.0);

        pl += inc_l;
        pr += inc_r;
        while (pl >= TWO_PI) pl -= TWO_PI;
        while (pr >= TWO_PI) pr -= TWO_PI;

        gain = synth_next_gain(gain, p.gain_target, p.gain_step);
    }

    phase->phase_l = pl;
    phase->phase_r = pr;
    return gain;
}
