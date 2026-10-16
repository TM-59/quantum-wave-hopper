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
