#include "synth.h"

#include <math.h>

#define TWO_PI 6.28318530717958647692

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

double synth_fill_buffer(int16_t* out, size_t frames,
                         SynthPhase* phase,
                         SynthFrameParams p)
{
    /* Per-sample phase increments (radians per sample). */
    double inc_l = TWO_PI * p.left_hz  / (double)SYNTH_SAMPLE_RATE;
    double inc_r = TWO_PI * p.right_hz / (double)SYNTH_SAMPLE_RATE;
    /* Honor AudioParams contract: hz == 0 means silent on that channel,
     * regardless of accumulated phase from prior calls. */
    int silent_l = (p.left_hz  == 0.0);
    int silent_r = (p.right_hz == 0.0);

    /* Modulation: 40 Hz pip train (Martorell et al. 2019).
     * Each mod cycle = 1/mod_hz seconds. The pip occupies the first 1 ms
     * of each cycle; the remainder is silence. */
    double inc_mod = (p.mod_hz > 0.0)
                   ? TWO_PI * p.mod_hz / (double)SYNTH_SAMPLE_RATE
                   : 0.0;
    double pip_end = TWO_PI * (0.001 * p.mod_hz);   /* 1 ms as fraction of cycle */

    double pl = phase->phase_l;
    double pr = phase->phase_r;
    double pm = phase->phase_mod;
    double gain = p.gain_start;

    for (size_t i = 0; i < frames; i++) {
        double sl = silent_l ? 0.0 : sin(pl) * p.volume * gain;
        double sr = silent_r ? 0.0 : sin(pr) * p.volume * gain;

        if (p.mod_hz > 0.0) {
            double env;
            if (pm >= pip_end) {
                env = 0.0;
            } else {
                /* Hanning window across the pip to eliminate spectral
                 * splatter from hard on/off gating. */
                double pip_pos = pm / pip_end;   /* 0..1 within pip */
                env = 0.5 * (1.0 - cos(TWO_PI * pip_pos));
            }
            sl *= env;
            sr *= env;
            pm += inc_mod;
            while (pm >= TWO_PI) pm -= TWO_PI;
        }

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
    phase->phase_mod = pm;
    return gain;
}

static uint32_t xorshift32(uint32_t* state) {
    uint32_t x = *state;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    *state = x;
    return x;
}

static double white_sample(NoiseChannel* ch) {
    return (double)(int32_t)xorshift32(&ch->seed) / 2147483647.0;
}

/* Paul Kellet's 1/f approximation — widely used, accurate to +-0.5 dB */
static double pink_sample(NoiseChannel* ch) {
    double w = white_sample(ch);
    ch->b0 = 0.99886 * ch->b0 + w * 0.0555179;
    ch->b1 = 0.99332 * ch->b1 + w * 0.0750759;
    ch->b2 = 0.96900 * ch->b2 + w * 0.1538520;
    ch->b3 = 0.86650 * ch->b3 + w * 0.3104856;
    ch->b4 = 0.55000 * ch->b4 + w * 0.5329522;
    ch->b5 = -0.7616 * ch->b5 - w * 0.0168980;
    double pink = ch->b0 + ch->b1 + ch->b2 + ch->b3
                + ch->b4 + ch->b5 + ch->b6 + w * 0.5362;
    ch->b6 = w * 0.115926;
    return pink * 0.11;
}

/* Brownian / red noise — integrated white noise with DC-blocking leak */
static double brown_sample(NoiseChannel* ch) {
    ch->brown += white_sample(ch) * 0.02;
    ch->brown *= 0.998;
    return ch->brown * 3.5;
}

static double noise_one(NoiseChannel* ch, int type) {
    switch (type) {
        case NOISE_WHITE: return white_sample(ch);
        case NOISE_PINK:  return pink_sample(ch);
        case NOISE_BROWN: return brown_sample(ch);
        default:          return 0.0;
    }
}

void noise_state_init(NoiseState* ns) {
    ns->left.seed  = 2463534242u;
    ns->right.seed = 1370291435u;
    ns->left.b0 = ns->left.b1 = ns->left.b2 = 0.0;
    ns->left.b3 = ns->left.b4 = ns->left.b5 = ns->left.b6 = 0.0;
    ns->left.brown = 0.0;
    ns->right.b0 = ns->right.b1 = ns->right.b2 = 0.0;
    ns->right.b3 = ns->right.b4 = ns->right.b5 = ns->right.b6 = 0.0;
    ns->right.brown = 0.0;
}

double synth_fill_noise(int16_t* out, size_t frames, NoiseState* ns,
                        int noise_type, double volume,
                        double gain_start, double gain_target, double gain_step)
{
    double gain = gain_start;
    for (size_t i = 0; i < frames; i++) {
        double sl = noise_one(&ns->left,  noise_type) * volume * gain;
        double sr = noise_one(&ns->right, noise_type) * volume * gain;
        if (sl >  1.0) sl =  1.0;
        if (sl < -1.0) sl = -1.0;
        if (sr >  1.0) sr =  1.0;
        if (sr < -1.0) sr = -1.0;
        out[2*i]     = (int16_t)(sl * 32767.0);
        out[2*i + 1] = (int16_t)(sr * 32767.0);
        gain = synth_next_gain(gain, gain_target, gain_step);
    }
    return gain;
}
