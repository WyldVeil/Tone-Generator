#ifndef SYNTH_H
#define SYNTH_H

#include <stdint.h>
#include <stddef.h>

/* Move current toward target by at most step. Never overshoot.
 * Returns the new gain value. Step must be positive. */
double synth_next_gain(double current, double target, double step);

#endif
