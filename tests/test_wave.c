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


/* ---- Two barriers (M2) ---- */

/* Closed-form amplitudes of one rectangular barrier on [0, b] (kappa = sqrt(2 (V - E)), imaginary above
 * the top): psi = e^{ikx} + r0 e^{-ikx} on the left, t0 e^{ikx} on the right. */
static void single_amplitudes(double k, double height, double b, double complex *t0, double complex *r0)
{
    double complex kappa = csqrt(2.0 * (height - 0.5 * k * k) + 0.0 * I);
    double complex den = ccosh(kappa * b) + I * (kappa * kappa - k * k) / (2.0 * k * kappa) * csinh(kappa * b);
    *t0 = cexp(-I * k * b) / den;
    *r0 = -I * (k * k + kappa * kappa) / (2.0 * k * kappa) * csinh(kappa * b) / den;
}

/* Fabry-Perot sum of the multiple reflections between two equal barriers a gap L apart:
 * T = |t0|^4 / |1 - r0^2 e^{2ikL}|^2. Independent of the transfer matrices. */
static double fabry_perot(double k, double height, double b, double gap)
{
    double complex t0, r0;
    single_amplitudes(k, height, b, &t0, &r0);
    double complex den = 1.0 - r0 * r0 * cexp(2.0 * I * k * gap);
    double tt = creal(t0 * conj(t0));
    return tt * tt / creal(den * conj(den));
}

static void test_layers_reduce_to_single_barrier(void)
{
    double worst = 0.0, worst_sum = 0.0;
    for (double e = 0.01; e < 2.0; e += 0.0137) {
        double k = sqrt(2.0 * e), edges[2] = {0.0, 2.0}, heights[1] = {0.6}, r;
        double t = layers_transmission(k, 1, edges, heights, &r);
        worst = fmax(worst, fabs(t - barrier_transmission(k, 0.6, 2.0)));
        worst_sum = fmax(worst_sum, fabs(t + r - 1.0));
        /* Two barriers with no gap are one barrier twice as wide. */
        worst = fmax(worst, fabs(double_barrier_transmission(k, 1.0, 1.0, 0.0) - barrier_transmission(k, 1.0, 2.0)));
        double e5[6] = {-1.0, 0.0, 1.0, 5.0, 6.0, 7.5}, h5[5] = {0.3, 1.0, 0.0, 1.0, 0.2}, r5;
        double t5 = layers_transmission(k, 5, e5, h5, &r5);
        worst_sum = fmax(worst_sum, fabs(t5 + r5 - 1.0));
    }
    CHECK(worst < 1e-10, "transfer matrices vs single-barrier formula: %.3e", worst);
    CHECK(worst_sum < 1e-10, "T + R - 1: %.3e", worst_sum);
}

static void test_double_barrier_matches_fabry_perot(void)
{
    double worst = 0.0;
    for (double e = 0.005; e < 2.0; e += 0.00731) {
        double k = sqrt(2.0 * e);
        if (fabs(e - 1.0) < 1e-6) continue;
        worst = fmax(worst, fabs(double_barrier_transmission(k, 1.0, 1.0, 4.0) - fabry_perot(k, 1.0, 1.0, 4.0)));
    }
    CHECK(worst < 1e-9, "double barrier vs Fabry-Perot closed form: %.3e", worst);
}

static void test_resonances_transmit_fully(void)
{
    double e1, g1, e2, g2;
    CHECK(double_barrier_resonance(1.0, 1.0, 4.0, 0.01, 1.0, &e1, &g1) == 0, "first resonance");
    CHECK(double_barrier_resonance(1.0, 1.0, 4.0, e1 + 3.0 * g1, 1.0, &e2, &g2) == 0, "second resonance");
    CHECK(fabs(double_barrier_transmission(sqrt(2.0 * e1), 1.0, 1.0, 4.0) - 1.0) < 1e-9, "T(E1) = 1");
    CHECK(fabs(double_barrier_transmission(sqrt(2.0 * e2), 1.0, 1.0, 4.0) - 1.0) < 1e-9, "T(E2) = 1");
    /* Between the two resonances one barrier alone passes about a third, the pair a few per cent. */
    double mid = 0.5 * (e1 + e2), k = sqrt(2.0 * mid);
    CHECK(double_barrier_transmission(k, 1.0, 1.0, 4.0) < 0.2 * barrier_transmission(k, 1.0, 1.0),
          "off resonance the pair is darker than one barrier");
    /* Both resonances lie below the infinite-well levels n^2 pi^2 / (2 L^2) of the gap. */
    CHECK(e1 < 0.5 * pow(3.14159265358979 / 4.0, 2) && e2 < 0.5 * pow(2.0 * 3.14159265358979 / 4.0, 2), "below the box levels");
    /* An asymmetric pair cannot reach 1 (T_max = 4 T_a T_b / (T_a + T_b)^2 for thin-barrier transmissions T_a, T_b). */
    double edges[4] = {0.0, 1.0, 5.0, 6.5}, heights[3] = {1.0, 0.0, 1.0}, best = 0.0;
    for (double e = 0.10; e < 0.25; e += 1e-5) best = fmax(best, layers_transmission(sqrt(2.0 * e), 3, edges, heights, NULL));
    CHECK(best < 0.9, "an asymmetric pair stays below 1: %.4f", best);
}

/* Largest gap between T and the Lorentzian 1 / (1 + 4 (E - E1)^2 / Gamma^2) within one width of E1. */
static double breit_wigner_gap(double width)
{
    double e1, g1, worst = 0.0;
    double_barrier_resonance(1.0, width, 4.0, 0.01, 1.0, &e1, &g1);
    for (double x = -1.0; x <= 1.0001; x += 0.01) {
        double e = e1 + x * g1, bw = 1.0 / (1.0 + 4.0 * x * x);
        worst = fmax(worst, fabs(double_barrier_transmission(sqrt(2.0 * e), 1.0, width, 4.0) - bw));
    }
    return worst;
}

static void test_breit_wigner_near_resonance(void)
{
    /* The line is a Lorentzian to first order in Gamma / E1; the gap shrinks with the width of the line. */
    double thin = breit_wigner_gap(1.0), thick = breit_wigner_gap(1.5);
    CHECK(thin < 0.06, "Breit-Wigner line within one width (barriers 1 wide): %.4f", thin);
    CHECK(thick < 0.4 * thin, "thicker barriers, narrower line, closer to Breit-Wigner: %.4f vs %.4f", thick, thin);
}

static void test_double_barrier_packet_matches_theory(void)
{
    double e1, g1;
    double_barrier_resonance(1.0, 1.0, 4.0, 0.01, 1.0, &e1, &g1);
    double k1 = sqrt(2.0 * e1);
    double sim = simulate_double_transmission(k1, 10.0, 1.0, 1.0, 4.0), th = double_packet_transmission(k1, 10.0, 1.0, 1.0, 4.0);
    CHECK(fabs(sim / th - 1.0) < 0.01, "on resonance: simulated %.5f vs packet theory %.5f", sim, th);
    CHECK(th < 0.5, "a packet broader than the line is not fully transmitted: %.4f", th);
    sim = simulate_double_transmission(0.9, 10.0, 1.0, 1.0, 4.0);
    th = double_packet_transmission(0.9, 10.0, 1.0, 1.0, 4.0);
    CHECK(fabs(sim / th - 1.0) < 0.01, "off resonance: simulated %.5f vs packet theory %.5f", sim, th);
}

static void test_lifetime_matches_width(void)
{
    double e1, g1;
    double_barrier_resonance(1.0, 1.0, 4.0, 0.01, 1.0, &e1, &g1);
    double rate = simulate_well_decay(1.0, 1.0, 4.0, 30.0, 180.0);
    CHECK(fabs(rate / g1 - 1.0) < 0.03, "decay rate %.5f vs width %.5f (lifetime %.1f vs hbar / Gamma %.1f)", rate, g1, 1.0 / rate, 1.0 / g1);
}

int main(void)
{
    test_norm_is_conserved();
    test_free_packet_moves_and_spreads();
    test_harmonic_ground_state_is_stationary();
    test_barrier_formula_limits();
    test_tunnelling_matches_theory();
    test_render_fits_the_screen();
    test_layers_reduce_to_single_barrier();
    test_double_barrier_matches_fabry_perot();
    test_resonances_transmit_fully();
    test_breit_wigner_near_resonance();
    test_double_barrier_packet_matches_theory();
    test_lifetime_matches_width();
    if (failures) {
        printf("%d check(s) failed\n", failures);
        return 1;
    }
    puts("all tests passed");
    return 0;
}
