/* The 2D solver (src/wave2d.c) against closed forms and conservation laws, and the browser's double-slit
 * experiment (src/slits.c) against Fraunhofer diffraction.
 * `./test_slits --trace` prints the native samples the browser smoke compares with WASM. */
#include "../src/slits.h"
#include "../src/wave2d.h"

#include <complex.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

#define PI 3.14159265358979323846

static int failures;
#define CHECK(cond, ...)                                                    \
    do {                                                                    \
        if (!(cond)) { failures++; printf("FAIL %s:%d: ", __FILE__, __LINE__); printf(__VA_ARGS__); printf("\n"); } \
        else { printf("ok   "); printf(__VA_ARGS__); printf("\n"); }   \
    } while (0)

/* A free packet: the centre moves at the grid's group velocity, the width in y follows the continuum law. */
static void test_free_packet(void)
{
    Wave2 w;
    double sx = 2.0, sy = 2.0, k = 2.0, dt = 0.01, t = 4.0;
    if (wave2_init(&w, 200, 121, -20.0, -12.0, 0.2, dt)) { failures++; return; }
    wave2_gaussian(&w, -8.0, 0.0, sx, sy, k, 0.0);
    double x0 = wave2_mean(&w, 0), e0 = wave2_energy(&w);
    for (int s = 0; s < (int)lround(t / dt); s++) wave2_step(&w);
    double vg = sin(k * 0.2) / 0.2, width = sqrt(wave2_variance(&w, 1)), theory = sy * sqrt(1.0 + pow(t / (2.0 * sy * sy), 2.0));
    CHECK(fabs((wave2_mean(&w, 0) - x0) / (vg * t) - 1.0) < 2e-3, "free packet: centre moved %.5f, grid group velocity gives %.5f", wave2_mean(&w, 0) - x0, vg * t);
    CHECK(fabs(width / theory - 1.0) < 2e-3, "free packet: width in y %.5f, continuum sigma sqrt(1 + (t/2s^2)^2) = %.5f", width, theory);
    CHECK(fabs(wave2_energy(&w) / e0 - 1.0) < 1e-10, "free packet: energy %.12f vs %.12f (V = 0: the factors commute)", wave2_energy(&w), e0);
    CHECK(fabs(wave2_mean(&w, 1)) < 1e-12, "free packet: no drift in y (%.2e)", wave2_mean(&w, 1));
    wave2_free(&w);
}

/* Every factor is a Cayley transform: the norm stays 1 to rounding, for the game's step and for a huge one. */
static void test_unitary(void)
{
    Wave2 w;
    if (wave2_init(&w, 128, 128, -12.8, -12.8, 0.2, 0.02)) { failures++; return; }
    wave2_gaussian(&w, -5.0, 0.0, 2.0, 3.0, 2.5, 0.0);
    wave2_set_slits(&w, 0.0, 0.8, 60.0, 4.4, 1.4, 3);
    double worst = 0.0;
    for (int s = 0; s < 400; s++) { wave2_step(&w); worst = fmax(worst, fabs(wave2_norm(&w) - 1.0)); }
    CHECK(worst < 1e-12, "unitary: largest |norm - 1| over 400 steps through the slits %.2e", worst);
    w.dt = 2.0;
    for (int s = 0; s < 50; s++) wave2_step(&w);
    CHECK(fabs(wave2_norm(&w) - 1.0) < 1e-12, "unitary: norm after 50 steps of dt = 2 is %.15f", wave2_norm(&w));
    wave2_free(&w);
}

static double distance(const Wave2 *a, const Wave2 *b)
{
    double s = 0.0;
    for (int k = 0; k < a->nx * a->ny; k++) { double complex d = a->psi[k] - b->psi[k]; s += creal(d * conj(d)); }
    return sqrt(s) * a->dx;
}

static void evolve_to(Wave2 *w, double dt, double t)
{
    wave2_init(w, 96, 97, -9.6, -9.6, 0.2, dt);
    wave2_gaussian(w, -3.0, 0.0, 1.5, 2.5, 2.5, 0.0);
    wave2_set_slits(w, 0.0, 0.8, 60.0, 4.4, 1.4, 3);
    for (int s = 0; s < (int)lround(t / dt); s++) wave2_step(w);
}

/* Second order in time: halving dt divides the error by about 4 (the slit wall included). */
static void test_second_order(void)
{
    Wave2 ref, w[3];
    double dts[3] = {0.01, 0.005, 0.0025}, err[3];
    evolve_to(&ref, 0.0003125, 1.6);
    for (int i = 0; i < 3; i++) { evolve_to(&w[i], dts[i], 1.6); err[i] = distance(&w[i], &ref); wave2_free(&w[i]); }
    wave2_free(&ref);
    double p1 = log(err[0] / err[1]) / log(2.0), p2 = log(err[1] / err[2]) / log(2.0);
    CHECK(p1 > 1.85 && p1 < 2.15 && p2 > 1.85 && p2 < 2.15, "second order: error %.3e %.3e %.3e, slopes %.3f %.3f", err[0], err[1], err[2], p1, p2);
}

/* The closed forms: the quadrature of uniform light equals the textbook formula, its zeros where they belong. */
static void test_fraunhofer_formulas(void)
{
    double worst = 0.0;
    for (int m = 0; m <= 200; m++) {
        double ky = -3.0 + 0.03 * m;
        for (int open = 1; open <= 3; open++)
            worst = fmax(worst, fabs(slit_fraunhofer(ky, 8.8, 2.2, 0.0, open) - slit_ideal(ky, 8.8, 2.2, open)));
    }
    CHECK(worst < 1e-9, "Fraunhofer quadrature with uniform light equals the closed form: largest difference %.2e", worst);
    CHECK(slit_ideal(2.0 * PI / 2.2, 8.8, 2.2, 1) < 1e-20, "single-slit zero at ky = 2 pi / a");
    CHECK(slit_ideal(PI / 8.8, 8.8, 2.2, 3) < 1e-20, "double-slit dark fringe at ky = pi / d");
    CHECK(fabs(slit_ideal(0.0, 8.8, 2.2, 3) / slit_ideal(0.0, 8.8, 2.2, 1) - 4.0) < 1e-12, "two slits in step are 4 times one slit at ky = 0");
    CHECK(fabs(slit_fraunhofer(0.7, 8.8, 2.2, 6.0, 1) - slit_fraunhofer(-0.7, 8.8, 2.2, 6.0, 1)) < 1e-12, "a real aperture diffracts symmetrically");
}

static double l1_screen(double (*f)(int), double (*g)(int))
{
    double s = 0.0, h = 2.0 * sl_value(SL_SCREEN_HALF) / SL_BINS;
    for (int m = 0; m < SL_BINS; m++) s += fabs(f(m) - g(m)) * h;
    return s;
}

/* Peak positions of a screen curve, refined by a parabola. */
static int peaks(double (*f)(int), double *where, int most)
{
    double h = 2.0 * sl_value(SL_SCREEN_HALF) / SL_BINS, top = 0.0;
    int n = 0;
    for (int m = 0; m < SL_BINS; m++) top = fmax(top, f(m));
    for (int m = 1; m < SL_BINS - 1 && n < most; m++) {
        double a = f(m - 1), b = f(m), c = f(m + 1);
        if (!(b >= a && b > c && b > 0.02 * top)) continue;
        double den = a - 2.0 * b + c;
        where[n++] = -sl_value(SL_SCREEN_HALF) + (m + 0.5 + (den != 0.0 ? 0.5 * (a - c) / den : 0.0)) * h;
    }
    return n;
}

static void finish(void)
{
    for (int guard = 0; guard < 1000 && !sl_value(SL_DONE); guard++) sl_step(50);
}

static double both_screen[SL_BINS];
static double saved(int m) { return both_screen[m]; }

static void test_double_slit(void)
{
    CHECK(sl_reset(0) == 0, "allocation");
    finish();
    double l1 = l1_screen(sl_screen, sl_screen_theory);
    CHECK(l1 < 0.10, "double slit: screen against Fraunhofer of the lit slits, L1 distance %.4f (of 2)", l1);
    CHECK(sl_value(SL_VISIBILITY) > 0.95, "double slit: central fringe visibility %.4f", sl_value(SL_VISIBILITY));
    double sim[16], th[16];
    int ns = peaks(sl_screen, sim, 16), nt = peaks(sl_screen_theory, th, 16);
    CHECK(ns == nt && ns >= 5, "double slit: %d bright fringes on the screen, Fraunhofer has %d", ns, nt);
    double worst = 0.0;
    for (int i = 0; i < ns && i < nt; i++) worst = fmax(worst, fabs(sim[i] - th[i]));
    CHECK(worst < 0.6, "double slit: fringes within %.3f of the Fraunhofer maxima (bin 1.0)", worst);
    double spacing = sl_value(SL_SPACING_MEASURED), exact = sl_value(SL_SPACING);
    CHECK(fabs(spacing / exact - 1.0) < 0.05, "double slit: first fringe at %.3f, L tan(asin(lambda / d)) = %.3f, paraxial lambda L / d = %.3f",
          spacing, exact, sl_value(SL_LAMBDA) * sl_value(SL_SCREEN_L) / sl_value(SL_SEP));
    CHECK(fabs(sl_value(SL_NORM) - 1.0) < 2e-3, "double slit: the absorbers took %.2e of the norm", 1.0 - sl_value(SL_NORM));
    for (int m = 0; m < SL_BINS; m++) both_screen[m] = sl_screen(m);
}

static void test_single_slit(void)
{
    sl_reset(1);
    finish();
    double l1 = l1_screen(sl_screen, sl_screen_theory);
    CHECK(l1 < 0.10, "slit B closed: screen against Fraunhofer of one lit slit, L1 %.4f", l1);
    CHECK(sl_value(SL_VISIBILITY) < 0.3, "slit B closed: no fringes, central 'visibility' %.3f is the envelope's slope (theory %.3f)",
          sl_value(SL_VISIBILITY), sl_value(SL_VISIBILITY_THEORY));
    double sim[16];
    CHECK(peaks(sl_screen, sim, 16) == 1, "slit B closed: one broad maximum");
    double p_one = sl_value(SL_P_BEYOND);
    sl_reset(2);
    finish();
    CHECK(fabs(sl_value(SL_P_BEYOND) / p_one - 1.0) < 1e-6, "the two slits pass the same probability %.6f %.6f (mirror images)", p_one, sl_value(SL_P_BEYOND));
}

static void test_which_path(void)
{
    sl_reset(3);
    finish();
    CHECK(sl_value(SL_COHERENCE) < 1e-15, "watched, phase uniform on [-pi, pi]: coherence %.1e", sl_value(SL_COHERENCE));
    CHECK(sl_value(SL_VISIBILITY) < 0.3, "watched: fringes gone, central visibility %.3f (theory %.3f)", sl_value(SL_VISIBILITY), sl_value(SL_VISIBILITY_THEORY));
    double l1 = l1_screen(sl_screen, sl_screen_theory);
    CHECK(l1 < 0.10, "watched: screen against the sum of the two one-slit patterns, L1 %.4f", l1);
    sl_scramble(0.0);
    double sup = l1_screen(sl_screen, saved);
    CHECK(sup < 0.03, "unwatched sum of the one-slit runs against the two-slit run (superposition), L1 %.4f", sup);
    CHECK(sl_value(SL_VISIBILITY) > 0.95, "phase 0: visibility back to %.4f", sl_value(SL_VISIBILITY));
    sl_scramble(PI / 2.0);
    double v = sl_value(SL_VISIBILITY), g = sl_value(SL_COHERENCE);
    CHECK(fabs(g - 2.0 / PI) < 1e-12 && fabs(v - sl_value(SL_VISIBILITY_THEORY)) < 0.03, "phase on [-pi/2, pi/2]: coherence %.4f, visibility %.4f (theory %.4f)",
          g, v, sl_value(SL_VISIBILITY_THEORY));
    sl_scramble(PI);
}

/* Born's rule: particles drawn one at a time follow the measured screen; the draw is a pure function of (seed, n). */
static void test_particles(void)
{
    sl_reset(0);
    finish();
    enum { COUNT = 40000, COARSE = 20 };
    double counts[COARSE] = {0}, half = sl_value(SL_SCREEN_HALF), h = 2.0 * half / SL_BINS;
    sl_seed(7);
    for (int n = 0; n < COUNT; n++) {
        double y = sl_particle(n);
        int c = (int)floor((y + half) / (2.0 * half) * COARSE);
        if (c >= 0 && c < COARSE) counts[c]++;
    }
    double worst = 0.0;
    for (int c = 0; c < COARSE; c++) {
        double p = 0.0;
        for (int m = c * (SL_BINS / COARSE); m < (c + 1) * (SL_BINS / COARSE); m++) p += sl_screen(m) * h;
        double sd = sqrt(COUNT * p * (1.0 - p)) + 1e-9;
        worst = fmax(worst, fabs(counts[c] - COUNT * p) / sd);
    }
    CHECK(worst < 4.5, "Born rule: 40,000 particles in 20 bins, largest deviation %.2f standard deviations", worst);
    double first = sl_particle(123);
    sl_seed(8);
    double other = sl_particle(123);
    sl_seed(7);
    CHECK(sl_particle(123) == first && other != first, "particle 123 is the same for the same seed (%.4f), another for another seed (%.4f)", first, other);
}

static void test_controls(void)
{
    sl_reset(0);
    sl_controls(9.0, 1.0, 9.0);
    CHECK(sl_value(SL_LAMBDA) == SL_LAMBDA_MAX && sl_value(SL_SEP) == SL_SEP_MIN && sl_value(SL_WIDTH) <= SL_SEP_MIN - 1.0 + 1e-9,
          "controls clamped: lambda %.2f sep %.2f width %.2f", sl_value(SL_LAMBDA), sl_value(SL_SEP), sl_value(SL_WIDTH));
    sl_controls(2.53, 9.1, 2.3);
    CHECK(fabs(sl_value(SL_LAMBDA) - 2.5) < 1e-12 && fabs(sl_value(SL_SEP) - 9.2) < 1e-12 && fabs(sl_value(SL_WIDTH) - 2.2) < 1e-12,
          "controls snapped: lambda %.2f sep %.2f width %.2f", sl_value(SL_LAMBDA), sl_value(SL_SEP), sl_value(SL_WIDTH));
    sl_scramble(-1.0);
    CHECK(sl_value(SL_SCRAMBLE) == 0.0, "scramble clamped to 0");
    sl_scramble(PI);
    sl_controls(2.5, 8.8, 2.2);
}

static void trace(void)
{
    printf("mode,step");
    for (int f = 0; f < SL_FIELDS; f++) printf(",f%d", f);
    printf(",image,s50,s100,s150,t100,i100,p0,p1\n");
    for (int mode = 0; mode < 4; mode++) {
        sl_reset(mode);
        sl_seed(3);
        for (;;) {
            sl_measure();
            unsigned char *img = sl_image();
            long sum = 0;
            for (int k = 0; k < 256 * 256 * 4; k++) sum += img[k];
            printf("%d,%d", mode, (int)sl_value(SL_STEPS));
            for (int f = 0; f < SL_FIELDS; f++) printf(",%.17g", sl_value(f));
            printf(",%ld,%.17g,%.17g,%.17g,%.17g,%.17g,%.17g,%.17g\n", sum, sl_screen(50), sl_screen(100), sl_screen(150), sl_screen_theory(100),
                   sl_screen_ideal(100), sl_particle(0), sl_particle(1));
            if (sl_value(SL_DONE)) break;
            sl_step(50);
        }
    }
}

int main(int argc, char **argv)
{
    if (argc > 1 && strcmp(argv[1], "--trace") == 0) { trace(); return 0; }
    test_free_packet();
    test_unitary();
    test_second_order();
    test_fraunhofer_formulas();
    test_controls();
    test_double_slit();
    test_single_slit();
    test_which_path();
    test_particles();
    printf(failures ? "double slit: %d failure(s)\n" : "double slit: all tests passed\n", failures);
    return failures != 0;
}
