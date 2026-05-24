#include "synth.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>

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

int main(void) {
    test_next_gain_moves_up_toward_target();
    test_next_gain_moves_down_toward_target();
    test_next_gain_does_not_overshoot_going_up();
    test_next_gain_does_not_overshoot_going_down();
    test_next_gain_at_target_returns_target();
    printf("test_synth (fade): 5 passed\n");
    return 0;
}
