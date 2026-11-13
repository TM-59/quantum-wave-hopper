#include "slits.h"
#include "wave2d.h"

#include <complex.h>
#include <math.h>
#include <stdint.h>
#include <stdlib.h>

#define PI 3.14159265358979323846
#define N 256
#define DX 0.2
#define DT 0.02
#define X0 (-25.6)
#define WALL_X 0.0
#define WALL_T 0.8
#define WALL_V 60.0
#define START_X (-12.0)
#define SIGMA_X 3.0
#define SIGMA_Y 6.0
#define LAYER 4.0
#define ABSORB 3.0
#define FROM (WALL_X + WALL_T + 1.0)    /* the spectrum is taken over x >= FROM */
#define END_X 9.0                         /* the run ends when a free packet's centre would be here */
#define SCREEN_L 100.0
#define SCREEN_HALF 100.0

static Wave2 a, b;                       /* b is used only in mode 3 (a = slit A only, b = slit B only) */
static int ready, mode, steps, max_steps, finished, measured_steps = -1;
static double lambda = 2.5, sep = 8.8, width = 2.2, scramble = PI, seed = 1.0;
static double screen[SL_BINS], theory[SL_BINS], ideal[SL_BINS], cdf[SL_BINS + 1];
static double visibility, visibility_theory, spacing_measured;
static double complex twiddle[SL_BINS][N];
static unsigned char image[N * N * 4];

static double wavenumber(void) { return 2.0 * PI / lambda; }
static double coherence(void) { return mode != 3 ? 1.0 : scramble < 1e-12 ? 1.0 : sin(scramble) / scramble; }
static double bin_y(int m) { return -SCREEN_HALF + (m + 0.5) * (2.0 * SCREEN_HALF / SL_BINS); }
/* Screen at distance L: a ray of transverse wavenumber ky leaves at sin(theta) = ky / k and lands at L tan(theta). */
static double bin_ky(int m) { double y = bin_y(m); return wavenumber() * y / sqrt(y * y + SCREEN_L * SCREEN_L); }
static double bin_jacobian(int m) { double y = bin_y(m), r2 = y * y + SCREEN_L * SCREEN_L; return wavenumber() * SCREEN_L * SCREEN_L / (r2 * sqrt(r2)); }

static void normalise(double *p)
{
    double s = 0.0, h = 2.0 * SCREEN_HALF / SL_BINS;
    for (int m = 0; m < SL_BINS; m++) s += p[m] * h;
    if (s > 0.0)
        for (int m = 0; m < SL_BINS; m++) p[m] /= s;
}

/* Fringe visibility (max - min) / (max + min) of p over the central bins within one fringe spacing of y = 0. */
static double central_visibility(const double *p)
{
    double span = SCREEN_L * tan(asin(fmin(0.99, lambda / sep))), hi = 0.0, lo = INFINITY;
    for (int m = 0; m < SL_BINS; m++)
        if (fabs(bin_y(m)) <= span) { hi = fmax(hi, p[m]); lo = fmin(lo, p[m]); }
    return hi + lo > 0.0 ? (hi - lo) / (hi + lo) : 0.0;
}

/* Distance from the central maximum to the first maximum above it, each refined by a parabola. */
static double first_spacing(const double *p)
{
    double h = 2.0 * SCREEN_HALF / SL_BINS, peaks[2];
    int found = 0, centre = SL_BINS / 2;
    if (p[centre - 1] > p[centre]) centre--;
    for (int m = centre; m < SL_BINS - 1 && found < 2; m++) {
        if (m == 0 || !(p[m] >= p[m - 1] && p[m] > p[m + 1])) continue;
        double den = p[m - 1] - 2.0 * p[m] + p[m + 1], shift = den != 0.0 ? 0.5 * (p[m - 1] - p[m + 1]) / den : 0.0;
        peaks[found++] = bin_y(m) + shift * h;
    }
    return found == 2 ? peaks[1] - peaks[0] : 0.0;
}

static void prepare_screen(void)
{
    for (int m = 0; m < SL_BINS; m++) {
        double ky = bin_ky(m);
        double complex turn = cexp(-I * ky * DX), ph = cexp(-I * ky * X0);
        for (int j = 0; j < N; j++) { twiddle[m][j] = ph; ph *= turn; }
        double f1 = slit_fraunhofer(ky, sep, width, SIGMA_Y, 1), f2 = slit_fraunhofer(ky, sep, width, SIGMA_Y, 2);
        double f3 = slit_fraunhofer(ky, sep, width, SIGMA_Y, 3), jac = bin_jacobian(m);
        int open = mode == 1 ? 1 : mode == 2 ? 2 : 3;
        theory[m] = jac * (mode == 3 ? f1 + f2 + coherence() * (f3 - f1 - f2) : slit_fraunhofer(ky, sep, width, SIGMA_Y, open));
        double i1 = slit_ideal(ky, sep, width, 1), i3 = slit_ideal(ky, sep, width, 3);
        ideal[m] = jac * (mode == 1 || mode == 2 ? i1 : mode == 3 ? 2.0 * i1 + coherence() * (i3 - 2.0 * i1) : i3);
    }
    normalise(theory);
    normalise(ideal);
    visibility_theory = central_visibility(theory);
}

static void set_cdf(void)
{
    cdf[0] = 0.0;
    for (int m = 0; m < SL_BINS; m++) cdf[m + 1] = cdf[m] + fmax(screen[m], 0.0);
}

void sl_measure(void)
{
    if (!ready) return;
    double gamma = coherence();
    for (int m = 0; m < SL_BINS; m++) {
        double s = 0.0;
        for (int i = 0; i < N; i++) {
            if (wave2_x(&a, i) < FROM - 1e-9) continue;
            double complex p = 0.0, q = 0.0;
            for (int j = 0; j < N; j++) p += a.psi[j * N + i] * twiddle[m][j];
            if (mode == 3) {
                for (int j = 0; j < N; j++) q += b.psi[j * N + i] * twiddle[m][j];
                s += creal(p * conj(p)) + creal(q * conj(q)) + 2.0 * gamma * creal(conj(p) * q);
            } else {
                s += creal(p * conj(p));
            }
        }
        screen[m] = s * DX * DX * DX / (2.0 * PI) * bin_jacobian(m);
    }
    normalise(screen);
    set_cdf();
    visibility = central_visibility(screen);
    spacing_measured = (mode == 0 || mode == 3) ? first_spacing(screen) : 0.0;
    measured_steps = steps;
}

void sl_launch(void)
{
    if (!ready) return;
    double k = wavenumber();
    wave2_gaussian(&a, START_X, 0.0, SIGMA_X, SIGMA_Y, k, 0.0);
    wave2_set_slits(&a, WALL_X, WALL_T, WALL_V, sep, width, mode == 1 || mode == 3 ? 1 : mode == 2 ? 2 : 3);
    if (mode == 3) {
        wave2_gaussian(&b, START_X, 0.0, SIGMA_X, SIGMA_Y, k, 0.0);
        wave2_set_slits(&b, WALL_X, WALL_T, WALL_V, sep, width, 2);
    }
    steps = 0;
    finished = 0;
    max_steps = (int)ceil((END_X - START_X) / (sin(k * DX) / DX) / DT);
    prepare_screen();
    for (int m = 0; m < SL_BINS; m++) screen[m] = 0.0;
    set_cdf();
    visibility = spacing_measured = 0.0;
    measured_steps = -1;
}

int sl_reset(int next_mode)
{
    if (!ready) {
        if (wave2_init(&a, N, N, X0, X0, DX, DT) || wave2_init(&b, N, N, X0, X0, DX, DT)) return 1;
        if (wave2_absorber(&a, LAYER, ABSORB) || wave2_absorber(&b, LAYER, ABSORB)) return 1;
        ready = 1;
    }
    mode = next_mode >= 0 && next_mode <= 3 ? next_mode : 0;
    sl_launch();
    return 0;
}

void sl_controls(double next_lambda, double next_sep, double next_width)
{
    if (!ready || !isfinite(next_lambda) || !isfinite(next_sep) || !isfinite(next_width)) return;
    lambda = fmax(SL_LAMBDA_MIN, fmin(SL_LAMBDA_MAX, round(next_lambda * 10.0) / 10.0));
    sep = fmax(SL_SEP_MIN, fmin(SL_SEP_MAX, round(next_sep / 0.4) * 0.4));
    double n = round((fmax(SL_WIDTH_MIN, fmin(SL_WIDTH_MAX, next_width)) - SL_WIDTH_MIN) / 0.4);
    while (n > 0 && SL_WIDTH_MIN + 0.4 * n > sep - 1.0 + 1e-9) n--;
    width = SL_WIDTH_MIN + 0.4 * n;
    sl_launch();
}

void sl_scramble(double phi)
{
    if (!ready || !isfinite(phi)) return;
    scramble = fmax(0.0, fmin(PI, phi));
    prepare_screen();
    if (measured_steps >= 0) sl_measure();
}

void sl_seed(double next) { if (isfinite(next)) seed = fmax(0.0, fmin(2147483647.0, floor(next))); }

void sl_step(int count)
{
    if (!ready || count < 0) return;
    if (count > 50) count = 50;
    for (int i = 0; i < count && !finished; i++) {
        wave2_step(&a);
        if (mode == 3) wave2_step(&b);
        if (++steps >= max_steps) finished = 1;
    }
    if (finished && measured_steps != steps) sl_measure();
}

double sl_value(int field)
{
    if (!ready) return 0.0;
    switch (field) {
    case SL_TIME: return steps * DT;
    case SL_MODE: return mode;
    case SL_LAMBDA: return lambda;
    case SL_K: return wavenumber();
    case SL_SEP: return sep;
    case SL_WIDTH: return width;
    case SL_SCRAMBLE: return scramble;
    case SL_COHERENCE: return coherence();
    case SL_NORM: return mode == 3 ? 0.5 * (wave2_norm(&a) + wave2_norm(&b)) : wave2_norm(&a);
    case SL_P_BEYOND: return mode == 3 ? wave2_prob_beyond(&a, FROM) + wave2_prob_beyond(&b, FROM) : wave2_prob_beyond(&a, FROM);
    case SL_DONE: return finished;
    case SL_T_MAX: return max_steps * DT;
    case SL_STEPS: return steps;
    case SL_NX: return N;
    case SL_NY: return N;
    case SL_X0: return X0;
    case SL_Y0: return X0;
    case SL_DX: return DX;
    case SL_WALL_X: return WALL_X;
    case SL_WALL_T: return WALL_T;
    case SL_SCREEN_L: return SCREEN_L;
    case SL_SCREEN_HALF: return SCREEN_HALF;
    case SL_SCREEN_BINS: return SL_BINS;
    case SL_SPACING: return SCREEN_L * tan(asin(fmin(0.99, lambda / sep)));
    case SL_DKY: return 2.0 * PI / sep;
    case SL_VISIBILITY: return visibility;
    case SL_VISIBILITY_THEORY: return visibility_theory;
    case SL_SPACING_MEASURED: return spacing_measured;
    case SL_MEASURED_AT: return measured_steps;
    case SL_SEED: return seed;
    default: return 0.0;
    }
}

/* Hue from the phase, brightness from the density (power 0.35 so the transmitted waves stay visible). */
static void paint(unsigned char *px, double hue, double value, int grey)
{
    double r, g, bl;
    if (grey) {
        r = 0.55 * value; g = 0.80 * value; bl = value;
    } else {
        double h = (hue + PI) / (2.0 * PI) * 6.0, f = h - floor(h);
        int sector = (int)floor(h) % 6;
        double q = 1.0 - f, rgb[6][3] = {{1, f, 0}, {q, 1, 0}, {0, 1, f}, {0, q, 1}, {f, 0, 1}, {1, 0, q}};
        r = rgb[sector][0] * value; g = rgb[sector][1] * value; bl = rgb[sector][2] * value;
    }
    px[0] = (unsigned char)lround(255.0 * fmin(1.0, r));
    px[1] = (unsigned char)lround(255.0 * fmin(1.0, g));
    px[2] = (unsigned char)lround(255.0 * fmin(1.0, bl));
    px[3] = 255;
}

static double cell_density(int k, int beyond)
{
    if (mode != 3) return creal(a.psi[k] * conj(a.psi[k]));
    double pa = creal(a.psi[k] * conj(a.psi[k])), pb = creal(b.psi[k] * conj(b.psi[k]));
    return beyond ? pa + pb + 2.0 * coherence() * creal(conj(a.psi[k]) * b.psi[k]) : 0.5 * (pa + pb);
}

unsigned char *sl_image(void)
{
    if (!ready) return image;
    double top = 1e-12;
    for (int k = 0; k < N * N; k++) top = fmax(top, cell_density(k, wave2_x(&a, k % N) >= WALL_X + WALL_T));
    for (int row = 0; row < N; row++) {
        int j = N - 1 - row;
        for (int i = 0; i < N; i++) {
            int k = j * N + i;
            unsigned char *px = image + 4 * (row * N + i);
            if (a.v[k] > 0.0 && (mode != 3 || b.v[k] > 0.0)) { px[0] = 240; px[1] = 228; px[2] = 200; px[3] = 255; continue; }
            double value = pow(cell_density(k, wave2_x(&a, i) >= WALL_X + WALL_T) / top, 0.35);
            paint(px, carg(a.psi[k]), value, mode == 3);
        }
    }
    return image;
}

double sl_screen(int bin) { return ready && bin >= 0 && bin < SL_BINS ? screen[bin] : 0.0; }
double sl_screen_theory(int bin) { return ready && bin >= 0 && bin < SL_BINS ? theory[bin] : 0.0; }
double sl_screen_ideal(int bin) { return ready && bin >= 0 && bin < SL_BINS ? ideal[bin] : 0.0; }

/* splitmix64: a counter-based draw, independent of the solver and of the order of the calls. */
static double uniform(uint64_t key)
{
    uint64_t z = key + 0x9E3779B97F4A7C15ull;
    z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ull;
    z = (z ^ (z >> 27)) * 0x94D049BB133111EBull;
    z ^= z >> 31;
    return (double)(z >> 11) * (1.0 / 9007199254740992.0);
}

double sl_particle(int n)
{
    if (!ready || n < 0 || cdf[SL_BINS] <= 0.0) return NAN;
    double u = uniform(((uint64_t)seed << 32) | (uint32_t)n) * cdf[SL_BINS];
    int lo = 0, hi = SL_BINS;
    while (hi - lo > 1) { int mid = (lo + hi) / 2; if (cdf[mid] <= u) lo = mid; else hi = mid; }
    double inside = screen[lo] > 0.0 ? (u - cdf[lo]) / screen[lo] : 0.5;
    double h = 2.0 * SCREEN_HALF / SL_BINS;
    return -SCREEN_HALF + (lo + fmin(1.0, fmax(0.0, inside))) * h;
}
