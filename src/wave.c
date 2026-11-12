#include "wave.h"

#include <math.h>
#include <stdlib.h>

int wave_init(Wave *w, int n, double x0, double dx, double dt)
{
    w->n = n; w->x0 = x0; w->dx = dx; w->dt = dt;
    w->psi = calloc(n, sizeof *w->psi);
    w->v = calloc(n, sizeof *w->v);
    w->scratch_c = calloc(n, sizeof *w->scratch_c);
    w->scratch_d = calloc(n, sizeof *w->scratch_d);
    if (!w->psi || !w->v || !w->scratch_c || !w->scratch_d) { wave_free(w); return 1; }
    return 0;
}

void wave_free(Wave *w)
{
    free(w->psi); free(w->v); free(w->scratch_c); free(w->scratch_d);
    w->psi = NULL; w->v = NULL; w->scratch_c = NULL; w->scratch_d = NULL;
}

double wave_x(const Wave *w, int j)
{
    return w->x0 + j * w->dx;
}

void wave_gaussian(Wave *w, double center, double sigma, double k0)
{
    for (int j = 0; j < w->n; j++) {
        double d = wave_x(w, j) - center;
        w->psi[j] = exp(-d * d / (4.0 * sigma * sigma)) * cexp(I * k0 * d);
    }
    double norm = sqrt(wave_norm(w));
    for (int j = 0; j < w->n; j++) w->psi[j] /= norm;
}

void wave_set_barrier(Wave *w, double left, double width, double height)
{
    for (int j = 0; j < w->n; j++) {
        double x = wave_x(w, j);
        w->v[j] = (x >= left - 1e-9 && x < left + width - 1e-9) ? height : 0.0;
    }
}

/* H psi at site j with hard walls (psi = 0 outside). */
static double complex h_psi(const Wave *w, int j)
{
    double inv = 1.0 / (w->dx * w->dx);
    double complex left = j > 0 ? w->psi[j - 1] : 0.0, right = j < w->n - 1 ? w->psi[j + 1] : 0.0;
    return (inv + w->v[j]) * w->psi[j] - 0.5 * inv * (left + right);
}

void wave_step(Wave *w)
{
    int n = w->n;
    double inv = 1.0 / (w->dx * w->dx), r = w->dt / 2.0;
    double complex off = -I * r * 0.5 * inv;   /* off-diagonal of (1 + i r H) */
    double complex *c = w->scratch_c, *d = w->scratch_d;

    /* Right-hand side (1 - i r H) psi, stored in d. */
    for (int j = 0; j < n; j++) d[j] = w->psi[j] - I * r * h_psi(w, j);

    /* Thomas algorithm for (1 + i r H) psi_new = d. */
    double complex diag = 1.0 + I * r * (inv + w->v[0]);
    c[0] = off / diag;
    d[0] = d[0] / diag;
    for (int j = 1; j < n; j++) {
        diag = 1.0 + I * r * (inv + w->v[j]);
        double complex m = diag - off * c[j - 1];
        c[j] = off / m;
        d[j] = (d[j] - off * d[j - 1]) / m;
    }
    w->psi[n - 1] = d[n - 1];
    for (int j = n - 2; j >= 0; j--) w->psi[j] = d[j] - c[j] * w->psi[j + 1];
}

double wave_norm(const Wave *w)
{
    double s = 0.0;
    for (int j = 0; j < w->n; j++) s += creal(w->psi[j] * conj(w->psi[j]));
    return s * w->dx;
}

double wave_mean_x(const Wave *w)
{
    double s = 0.0;
    for (int j = 0; j < w->n; j++) s += wave_x(w, j) * creal(w->psi[j] * conj(w->psi[j]));
    return s * w->dx / wave_norm(w);
}

double wave_variance_x(const Wave *w)
{
    double m = wave_mean_x(w), s = 0.0;
    for (int j = 0; j < w->n; j++) {
        double d = wave_x(w, j) - m;
        s += d * d * creal(w->psi[j] * conj(w->psi[j]));
    }
    return s * w->dx / wave_norm(w);
}

double wave_energy(const Wave *w)
{
    double s = 0.0;
    for (int j = 0; j < w->n; j++) s += creal(conj(w->psi[j]) * h_psi(w, j));
    return s * w->dx / wave_norm(w);
}

double wave_prob_right(const Wave *w, double x)
{
    double s = 0.0;
    for (int j = 0; j < w->n; j++)
        if (wave_x(w, j) >= x) s += creal(w->psi[j] * conj(w->psi[j]));
    return s * w->dx / wave_norm(w);
}

double barrier_transmission(double k, double height, double width)
{
    double e = 0.5 * k * k;
    if (fabs(e - height) < 1e-12) return 1.0 / (1.0 + 0.5 * height * width * width);
    if (e < height) {
        double kappa = sqrt(2.0 * (height - e)), s = sinh(kappa * width);
        return 1.0 / (1.0 + height * height * s * s / (4.0 * e * (height - e)));
    }
    double q = sqrt(2.0 * (e - height)), s = sin(q * width);
    return 1.0 / (1.0 + height * height * s * s / (4.0 * e * (e - height)));
}

double packet_transmission(double k0, double sigma, double height, double width)
{
    /* |phi(k)|^2 is Gaussian with standard deviation 1 / (2 sigma). */
    double sk = 0.5 / sigma, num = 0.0, den = 0.0;
    int steps = 800;
    for (int i = 0; i <= steps; i++) {
        double k = k0 - 6.0 * sk + 12.0 * sk * i / steps;
        if (k <= 0.0) continue;
        double wgt = exp(-0.5 * (k - k0) * (k - k0) / (sk * sk));
        num += wgt * barrier_transmission(k, height, width);
        den += wgt;
    }
    return num / den;
}

double simulate_transmission(double k0, double sigma, double height, double width)
{
    Wave w;
    int n = 4000;
    double dx = 0.1, start = -100.0;
    if (wave_init(&w, n, -200.0, dx, 0.05)) return -1.0;
    wave_gaussian(&w, start, sigma, k0);
    wave_set_barrier(&w, 0.0, width, height);
    double tmax = (fabs(start) + 80.0) / k0;
    for (double t = 0.0; t < tmax; t += w.dt) wave_step(&w);
    double p = wave_prob_right(&w, width);
    wave_free(&w);
    return p;
}

/* ---- Two barriers (M2): resonant tunnelling ---- */

void wave_set_double_barrier(Wave *w, double left, double width, double gap, double height)
{
    double second = left + width + gap;
    for (int j = 0; j < w->n; j++) {
        double x = wave_x(w, j);
        int in1 = x >= left - 1e-9 && x < left + width - 1e-9;
        int in2 = x >= second - 1e-9 && x < second + width - 1e-9;
        w->v[j] = in1 || in2 ? height : 0.0;
    }
}

double wave_prob_between(const Wave *w, double from, double to)
{
    double s = 0.0;
    for (int j = 0; j < w->n; j++) {
        double x = wave_x(w, j);
        if (x >= from - 1e-9 && x < to - 1e-9) s += creal(w->psi[j] * conj(w->psi[j]));
    }
    return s * w->dx / wave_norm(w);
}

/* Local wavenumber sqrt(2 (E - V)): real above the step, i kappa below it, never exactly 0. */
static double complex local_k(double e, double v)
{
    double complex q = csqrt(2.0 * (e - v) + 0.0 * I);
    return cabs(q) < 1e-9 ? 1e-9 : q;
}

double layers_transmission(double k, int layers, const double *edges, const double *heights, double *reflection)
{
    /* In each region psi = A e^{iKx} + B e^{-iKx}. Start beyond the last edge with (A, B) = (1, 0) and
     * carry the amplitudes leftwards across every interface by continuity of psi and psi'. */
    double e = 0.5 * k * k;
    double complex a = 1.0, b = 0.0, kr = k;
    for (int i = layers; i >= 0; i--) {
        double x = edges[i];
        double complex kl = i > 0 ? local_k(e, heights[i - 1]) : k, rho = kr / kl;
        double complex ar = a * cexp(I * kr * x), br = b * cexp(-I * kr * x);
        a = 0.5 * ((1.0 + rho) * ar + (1.0 - rho) * br) * cexp(-I * kl * x);
        b = 0.5 * ((1.0 - rho) * ar + (1.0 + rho) * br) * cexp(I * kl * x);
        kr = kl;
    }
    double t = 1.0 / creal(a * conj(a));
    if (reflection) *reflection = creal(b * conj(b)) * t;
    return t;
}

double double_barrier_transmission(double k, double height, double width, double gap)
{
    double edges[4] = {0.0, width, width + gap, 2.0 * width + gap}, heights[3] = {height, 0.0, height};
    return layers_transmission(k, 3, edges, heights, NULL);
}

static double t_of_e(double e, double height, double width, double gap)
{
    return double_barrier_transmission(sqrt(2.0 * e), height, width, gap);
}

/* The energy on (lo, hi) where T crosses 0.5, T(lo) and T(hi) on opposite sides of it. */
static double half_crossing(double lo, double hi, double height, double width, double gap)
{
    int rising = t_of_e(lo, height, width, gap) < 0.5;
    for (int i = 0; i < 80; i++) {
        double mid = 0.5 * (lo + hi);
        if ((t_of_e(mid, height, width, gap) < 0.5) == rising) lo = mid; else hi = mid;
    }
    return 0.5 * (lo + hi);
}

int double_barrier_resonance(double height, double width, double gap, double elo, double ehi,
                             double *energy, double *fwhm)
{
    /* Scan for the first local maximum of T, refine it by golden section, then bisect the two
     * half-maximum points. A peak narrower than the scan step (window / 20000) can be missed. */
    enum { SCAN = 20000 };
    double step = (ehi - elo) / SCAN;
    double prev = t_of_e(elo, height, width, gap), cur = t_of_e(elo + step, height, width, gap);
    for (int i = 1; i < SCAN; i++) {
        double e = elo + step * i, next = t_of_e(e + step, height, width, gap);
        if (cur > prev && cur >= next) {
            double a = e - step, b = e + step, g = 0.5 * (sqrt(5.0) - 1.0);
            for (int it = 0; it < 200; it++) {
                double c = b - g * (b - a), d = a + g * (b - a);
                if (t_of_e(c, height, width, gap) > t_of_e(d, height, width, gap)) b = d; else a = c;
            }
            double peak = 0.5 * (a + b), lo = peak, hi = peak;
            if (t_of_e(peak, height, width, gap) > 0.5) {
                while (lo > elo && t_of_e(lo, height, width, gap) > 0.5) lo -= step;
                while (hi < ehi && t_of_e(hi, height, width, gap) > 0.5) hi += step;
                if (lo <= elo || hi >= ehi) return 1;
                if (energy) *energy = peak;
                if (fwhm) *fwhm = half_crossing(peak, hi, height, width, gap) - half_crossing(lo, peak, height, width, gap);
                return 0;
            }
        }
        prev = cur;
        cur = next;
    }
    return 1;
}

double double_packet_transmission(double k0, double sigma, double height, double width, double gap)
{
    /* Same quadrature as packet_transmission, finer because the resonance is narrow. */
    double sk = 0.5 / sigma, num = 0.0, den = 0.0;
    int steps = 6000;
    for (int i = 0; i <= steps; i++) {
        double k = k0 - 6.0 * sk + 12.0 * sk * i / steps;
        if (k <= 0.0) continue;
        double wgt = exp(-0.5 * (k - k0) * (k - k0) / (sk * sk));
        num += wgt * double_barrier_transmission(k, height, width, gap);
        den += wgt;
    }
    return num / den;
}

double simulate_double_transmission(double k0, double sigma, double height, double width, double gap)
{
    /* The slow side of the packet (k0 - 3 / (2 sigma)) must arrive, and what lingers between the
     * barriers must leak out: run until the well holds less than 1e-6, at most 400 time units more.
     * The grid is wide enough that the fast side never meets the walls in that time. */
    Wave w;
    double dx = 0.05, start = -6.0 * sigma - 20.0, span = 2.0 * width + gap, sk = 0.5 / sigma;
    double slow = fmax(k0 - 3.0 * sk, 0.25 * k0), fast = k0 + 3.0 * sk;
    double t0 = (fabs(start) + span + 3.0 * sigma) / slow, tcap = t0 + 400.0;
    double xl = start - 6.0 * sigma - fast * tcap, xr = span + 6.0 * sigma + fast * tcap;
    int n = (int)((xr - xl) / dx);
    if (wave_init(&w, n, xl, dx, 0.05)) return -1.0;
    wave_gaussian(&w, start, sigma, k0);
    wave_set_double_barrier(&w, 0.0, width, gap, height);
    double t = 0.0;
    for (; t < t0; t += w.dt) wave_step(&w);
    for (; t < tcap && wave_prob_between(&w, width, width + gap) > 1e-6; t += w.dt) wave_step(&w);
    double p = wave_prob_right(&w, span);
    wave_free(&w);
    return p;
}

double simulate_well_decay(double height, double width, double gap, double t1, double t2)
{
    Wave w;
    double dx = 0.05, reach = 30.0 + 2.5 * t2;   /* what leaks out runs at about sqrt(2 E) < 2.5 */
    int n = (int)(2.0 * reach / dx);
    if (wave_init(&w, n, -reach, dx, 0.05)) return -1.0;
    double a = width, b = width + gap;   /* the well, the first barrier starting at 0 */
    wave_set_double_barrier(&w, 0.0, width, gap, height);
    for (int j = 0; j < w.n; j++) {
        double x = wave_x(&w, j);
        w.psi[j] = x > a && x < b ? sin(3.14159265358979323846 * (x - a) / gap) : 0.0;
    }
    double norm = sqrt(wave_norm(&w));
    for (int j = 0; j < w.n; j++) w.psi[j] /= norm;
    /* Least-squares slope of ln P(t) on [t1, t2]. */
    double sx = 0.0, sy = 0.0, sxx = 0.0, sxy = 0.0;
    int m = 0, steps = (int)(t2 / w.dt + 0.5);
    for (int k = 0; k <= steps; k++) {
        double t = k * w.dt;
        if (t >= t1 - 1e-9) {
            double y = log(wave_prob_between(&w, a, b));
            sx += t; sy += y; sxx += t * t; sxy += t * y; m++;
        }
        if (k < steps) wave_step(&w);
    }
    wave_free(&w);
    return -(m * sxy - sx * sy) / (m * sxx - sx * sx);
}
