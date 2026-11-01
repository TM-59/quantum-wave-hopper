/* Browser adapter checks: it must be the same solver as the terminal game, not a re-implementation. */
#include "../src/observer.h"
#include "../src/wave.h"

#include <complex.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

static int checks, failures;
#define CHECK(cond, ...) do { checks++; if (!(cond)) { failures++; printf("FAIL %s:%d ", __FILE__, __LINE__); \
    printf(__VA_ARGS__); printf("\n"); } } while (0)

static void trace(void)
{
    printf("mode,step");
    for (int f = 0; f < QW_FIELDS; f++) printf(",f%d", f);
    printf(",d0,d150,d195,d200,d300,r150,r200\n");
    for (int m = 0; m < 3; m++) {
        qw_reset(m);
        for (int s = 0; s <= QW_STEPS_PER_RUN; s++) {
            if (s % 100 == 0) {
                printf("%d,%d", m, s);
                for (int f = 0; f < QW_FIELDS; f++) printf(",%.17g", qw_value(f));
                int bins[] = {0, 150, 195, 200, 300};
                for (int i = 0; i < 5; i++) printf(",%.17g", qw_density(bins[i]));
                printf(",%.17g,%.17g\n", qw_real(150), qw_real(200));
            }
            if (m == 0 && s == 300) qw_controls(10.0, 0.7);
            if (m == 0 && s == 600) qw_controls(10.0, 0.4);
            qw_step(1);
        }
    }
}

int main(int argc, char **argv)
{
    if (argc > 1 && strcmp(argv[1], "--trace") == 0) { trace(); return 0; }

    /* 1. The adapter evolves exactly the same psi as the Wave API used by the terminal game. */
    Wave ref;
    CHECK(wave_init(&ref, 3900, -195.0, 0.1, 0.05) == 0, "reference grid");
    wave_gaussian(&ref, -100.0, 10.0, 1.0);
    wave_set_barrier(&ref, 0.0, 2.0, 0.6);
    CHECK(qw_reset(1) == 0, "reset");
    for (int s = 0; s < 1800; s++) wave_step(&ref);
    qw_step(400); qw_step(400); qw_step(400); qw_step(400); qw_step(200);
    double p = 0.0;
    for (int j = 0; j < ref.n; j++) p += creal(ref.psi[j] * conj(ref.psi[j]));
    p *= ref.dx;
    CHECK(fabs(qw_value(QW_NORM) - p) == 0.0, "norm identical to Wave API: %.17g vs %.17g", qw_value(QW_NORM), p);
    CHECK(fabs(qw_value(QW_P_RIGHT) - wave_prob_right(&ref, 2.0)) == 0.0, "prob right identical");
    int per = 3900 / QW_BIN_COUNT;
    double d = 0.0;
    for (int j = 150 * per; j < 151 * per; j++) d += creal(ref.psi[j] * conj(ref.psi[j]));
    CHECK(fabs(qw_density(150) - d / per) == 0.0, "binned density identical");
    CHECK(qw_real(150) == creal(ref.psi[150 * per + per / 2]), "Re psi sample identical");
    wave_free(&ref);
    CHECK(fabs(qw_value(QW_TIME) - 90.0) < 1e-12, "time 90 after 1800 steps");

    /* 2. Physics through the adapter: norm conservation, regions add up, tunnelling vs theory. */
    qw_step(400); qw_step(400); qw_step(400); qw_step(300);
    CHECK(qw_value(QW_DONE) == 1.0, "run finishes at %d steps", QW_STEPS_PER_RUN);
    qw_step(50);
    CHECK(fabs(qw_value(QW_TIME) - QW_STEPS_PER_RUN * 0.05) < 1e-9, "no steps past the run");
    CHECK(fabs(qw_value(QW_NORM) - 1.0) < 1e-10, "norm conserved: %.3e", qw_value(QW_NORM) - 1.0);
    double sum = qw_value(QW_P_LEFT) + qw_value(QW_P_INSIDE) + qw_value(QW_P_RIGHT);
    CHECK(fabs(sum - 1.0) < 1e-12, "regions sum to 1: %.3e", sum - 1.0);
    double t = qw_value(QW_P_RIGHT), th = qw_value(QW_THEORY);
    CHECK(fabs(t - th) < 0.01, "standard barrier T %.4f vs theory %.4f", t, th);
    printf("standard barrier: T sim %.4f theory %.4f\n", t, th);
    double bins = 0.0;
    for (int b = 0; b < QW_BIN_COUNT; b++) bins += qw_density(b);
    CHECK(fabs(bins * per * 0.1 - 1.0) < 1e-9, "bins integrate to the norm");

    /* 3. Half-through demonstration: the goal of the game is reachable and the height is theory-based. */
    double h = qw_half_height();
    CHECK(fabs(packet_transmission(1.0, 10.0, h, 2.0) - 0.5) < 1e-9, "half height gives theory 0.5");
    qw_reset(2);
    CHECK(qw_value(QW_HEIGHT) == h, "demo uses the half height");
    for (int i = 0; i < 10; i++) qw_step(400);
    double half = qw_value(QW_P_RIGHT);
    CHECK(fabs(half - 0.5) < 0.01, "half demo transmits %.4f", half);
    printf("half-through: V %.6f T sim %.4f\n", h, half);

    /* 4. Controls: demos are locked, manual clamps, NaN ignored, grid-aligned barrier position. */
    qw_controls(20.0, 1.0);
    CHECK(qw_value(QW_LEFT) == 0.0 && qw_value(QW_HEIGHT) == h, "demo locks controls");
    qw_reset(0);
    qw_controls(7.0, 0.8);
    CHECK(qw_value(QW_LEFT) == 8.0 && qw_value(QW_HEIGHT) == 0.8, "manual barrier moves on 2-unit steps");
    qw_controls(500.0, -3.0);
    CHECK(qw_value(QW_LEFT) == 60.0 && qw_value(QW_HEIGHT) == 0.0, "manual clamps");
    qw_controls(NAN, 0.5);
    CHECK(qw_value(QW_LEFT) == 60.0 && qw_value(QW_HEIGHT) == 0.0, "NaN ignored");
    qw_step(-5);
    CHECK(qw_value(QW_TIME) == 0.0, "negative steps ignored");
    CHECK(qw_density(-1) == 0.0 && qw_density(QW_BIN_COUNT) == 0.0 && qw_value(99) == 0.0, "out of range");
    qw_controls(0.0, 0.0);
    for (int i = 0; i < 10; i++) qw_step(400);
    CHECK(qw_value(QW_P_RIGHT) > 0.99, "no barrier: packet passes (%.4f)", qw_value(QW_P_RIGHT));
    qw_launch();
    CHECK(qw_value(QW_TIME) == 0.0 && fabs(qw_value(QW_MEAN_X) + 100.0) < 1e-6, "relaunch resets packet");

    printf("observer: %d checks, %d failures\n", checks, failures);
    return failures != 0;
}
