#include <math.h>
#include <stdio.h>

#include "../src/render.h"
#include "../src/wave.h"

static int failures;

#define CHECK(cond, ...)                                      \
    do {                                                      \
        if (!(cond)) {                                        \
            failures++;                                       \
            printf("FAIL %s:%d: ", __FILE__, __LINE__);       \
            printf(__VA_ARGS__);                              \
            printf("\n");                                     \
        }                                                     \
    } while (0)

static void test_norm_is_conserved(void)
{
    Wave w;
    CHECK(wave_init(&w, 2000, -100.0, 0.1, 0.05) == 0, "allocation failed");
    wave_gaussian(&w, -30.0, 5.0, 1.5);
    wave_set_barrier(&w, 0.0, 2.0, 1.0);
    for (int k = 0; k < 1000; k++) wave_step(&w);
    CHECK(fabs(wave_norm(&w) - 1.0) < 1e-10, "norm %.12f", wave_norm(&w));
    wave_free(&w);
}

static void test_free_packet_moves_and_spreads(void)
{
    Wave w;
    CHECK(wave_init(&w, 3000, -150.0, 0.1, 0.02) == 0, "allocation failed");
    double sigma = 2.0, k0 = 1.0, t = 10.0;
    wave_gaussian(&w, -50.0, sigma, k0);
    double e0 = wave_energy(&w);
    for (int k = 0; k < (int)(t / w.dt); k++) wave_step(&w);
    CHECK(fabs(wave_mean_x(&w) - (-50.0 + k0 * t)) < 0.05, "centre %.3f vs %.3f", wave_mean_x(&w), -50.0 + k0 * t);
    double expected = sigma * sqrt(1.0 + pow(t / (2.0 * sigma * sigma), 2.0));
    CHECK(fabs(sqrt(wave_variance_x(&w)) / expected - 1.0) < 0.01, "width %.4f vs %.4f", sqrt(wave_variance_x(&w)), expected);
    CHECK(fabs(wave_energy(&w) - e0) / e0 < 1e-4, "energy %.6f -> %.6f", e0, wave_energy(&w));
    CHECK(fabs(e0 - (0.5 * k0 * k0 + 1.0 / (8.0 * sigma * sigma))) < 1e-3, "packet energy %.5f", e0);
    wave_free(&w);
}

static void test_harmonic_ground_state_is_stationary(void)
{
    Wave w;
    CHECK(wave_init(&w, 800, -20.0, 0.05, 0.01) == 0, "allocation failed");
    for (int j = 0; j < w.n; j++) {
        double x = wave_x(&w, j);
        w.v[j] = 0.5 * x * x;
        w.psi[j] = exp(-x * x / 2.0);
    }
    double norm = sqrt(wave_norm(&w));
    for (int j = 0; j < w.n; j++) w.psi[j] /= norm;
    CHECK(fabs(wave_energy(&w) - 0.5) < 1e-3, "ground energy %.5f", wave_energy(&w));
    double p0 = creal(w.psi[400] * conj(w.psi[400]));
    for (int k = 0; k < 1000; k++) wave_step(&w);
    double p1 = creal(w.psi[400] * conj(w.psi[400]));
    CHECK(fabs(p1 / p0 - 1.0) < 1e-3, "density at the centre changed: %.6f -> %.6f", p0, p1);
    wave_free(&w);
}

static void test_barrier_formula_limits(void)
{
    CHECK(fabs(barrier_transmission(2.0, 0.0, 3.0) - 1.0) < 1e-12, "no barrier means full transmission");
    CHECK(barrier_transmission(0.5, 5.0, 4.0) < 1e-6, "thick tall barrier should be opaque");
    /* resonance: sin(q a) = 0 gives T = 1 above the barrier */
    double q = 3.14159265358979 / 2.0;
    double height = 1.0, width = 2.0, k = sqrt(2.0 * (height + 0.5 * q * q));
    CHECK(fabs(barrier_transmission(k, height, width) - 1.0) < 1e-9, "resonant transmission should be 1");
}

static void test_tunnelling_matches_theory(void)
{
    double sigma = 10.0, k0 = 1.0, height = 0.6, width = 2.0;
    double numeric = simulate_transmission(k0, sigma, height, width);
    double theory = packet_transmission(k0, sigma, height, width);
    CHECK(numeric > 0.0, "simulation failed");
    CHECK(fabs(numeric - theory) / theory < 0.05, "T numeric %.4f vs theory %.4f", numeric, theory);
}

static void test_render_fits_the_screen(void)
{
    Wave w;
    CHECK(wave_init(&w, 500, -25.0, 0.1, 0.05) == 0, "allocation failed");
    wave_gaussian(&w, 0.0, 3.0, 0.0);
    wave_set_barrier(&w, 5.0, 2.0, 1.0);
    static char buf[RENDER_COLS * RENDER_ROWS * 2];
    size_t used = render_frame(&w, 0.1, buf, sizeof buf);
    int lines = 0;
    for (size_t i = 0; i < used; i++) lines += buf[i] == '\n';
    CHECK(lines == RENDER_ROWS, "expected %d lines, got %d", RENDER_ROWS, lines);
    CHECK(render_frame(&w, 0.1, buf, 8) == 0, "tiny buffer must be refused");
    wave_free(&w);
}

int main(void)
{
    test_norm_is_conserved();
    test_free_packet_moves_and_spreads();
    test_harmonic_ground_state_is_stationary();
    test_barrier_formula_limits();
    test_tunnelling_matches_theory();
    test_render_fits_the_screen();
    if (failures) {
        printf("%d check(s) failed\n", failures);
        return 1;
    }
    puts("all tests passed");
    return 0;
}
