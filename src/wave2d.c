#include "wave2d.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

#define PI 3.14159265358979323846

int wave2_init(Wave2 *w, int nx, int ny, double x0, double y0, double dx, double dt)
{
    memset(w, 0, sizeof *w);
    if (nx < 3 || ny < 3) return 1;
    int n = nx > ny ? nx : ny;
    w->nx = nx;
    w->ny = ny;
    w->x0 = x0;
    w->y0 = y0;
    w->dx = dx;
    w->dt = dt;
    w->psi = calloc((size_t)nx * ny, sizeof *w->psi);
    w->v = calloc((size_t)nx * ny, sizeof *w->v);
    w->line = calloc((size_t)n, sizeof *w->line);
    w->rhs = calloc((size_t)n, sizeof *w->rhs);
    w->c = calloc((size_t)n, sizeof *w->c);
    w->d = calloc((size_t)n, sizeof *w->d);
    if (!w->psi || !w->v || !w->line || !w->rhs || !w->c || !w->d) { wave2_free(w); return 1; }
    return 0;
}

void wave2_free(Wave2 *w)
{
    free(w->psi);
    free(w->v);
    free(w->damp);
    free(w->line);
    free(w->rhs);
    free(w->c);
    free(w->d);
    memset(w, 0, sizeof *w);
}

double wave2_x(const Wave2 *w, int i) { return w->x0 + i * w->dx; }
double wave2_y(const Wave2 *w, int j) { return w->y0 + j * w->dx; }

void wave2_gaussian(Wave2 *w, double cx, double cy, double sx, double sy, double kx, double ky)
{
    for (int j = 0; j < w->ny; j++) {
        double y = wave2_y(w, j) - cy;
        for (int i = 0; i < w->nx; i++) {
            double x = wave2_x(w, i) - cx;
            w->psi[j * w->nx + i] = exp(-x * x / (4.0 * sx * sx) - y * y / (4.0 * sy * sy)) * cexp(I * (kx * x + ky * y));
        }
    }
    double s = sqrt(wave2_norm(w));
    if (s > 0.0)
        for (int k = 0; k < w->nx * w->ny; k++) w->psi[k] /= s;
}

/* Length of [a, b) that lies inside [lo, hi). */
static double overlap(double a, double b, double lo, double hi)
{
    double l = fmax(a, lo), h = fmin(b, hi);
    return h > l ? h - l : 0.0;
}

void wave2_set_slits(Wave2 *w, double wall_x, double thickness, double height, double sep, double width, int open)
{
    for (int j = 0; j < w->ny; j++) {
        double y = wave2_y(w, j), lo = y - 0.5 * w->dx, hi = y + 0.5 * w->dx, inside = 0.0;
        if (open & 1) inside += overlap(lo, hi, 0.5 * sep - 0.5 * width, 0.5 * sep + 0.5 * width);
        if (open & 2) inside += overlap(lo, hi, -0.5 * sep - 0.5 * width, -0.5 * sep + 0.5 * width);
        int hole = inside > 0.5 * w->dx;
        for (int i = 0; i < w->nx; i++) {
            double x = wave2_x(w, i);
            w->v[j * w->nx + i] = !hole && x >= wall_x - 1e-9 && x < wall_x + thickness - 1e-9 ? height : 0.0;
        }
    }
}

int wave2_absorber(Wave2 *w, double layer, double strength)
{
    free(w->damp);
    w->damp = NULL;
    if (strength <= 0.0 || layer <= 0.0) return 0;
    w->damp = malloc((size_t)w->nx * w->ny * sizeof *w->damp);
    if (!w->damp) return 1;
    double xl = w->x0, xh = wave2_x(w, w->nx - 1), yl = w->y0, yh = wave2_y(w, w->ny - 1);
    for (int j = 0; j < w->ny; j++)
        for (int i = 0; i < w->nx; i++) {
            double x = wave2_x(w, i), y = wave2_y(w, j);
            double depth = fmax(fmax(xl + layer - x, x - (xh - layer)), fmax(yl + layer - y, y - (yh - layer)));
            double s = fmin(1.0, fmax(0.0, depth / layer));
            w->damp[j * w->nx + i] = exp(-strength * s * s * w->dt);
        }
    return 0;
}

/* One Cayley factor (1 + i s A / 2)^-1 (1 - i s A / 2) along a line of n points with stride `stride`,
 * A = -1/2 D2 + V/2, Dirichlet ends (psi = 0 outside). Thomas algorithm, no pivoting: the matrix
 * 1 + i s A / 2 has a diagonal of modulus > the sum of its off-diagonals. */
static void cayley_line(Wave2 *w, double complex *p, const double *v, int n, int stride, double s)
{
    const double h2 = 1.0 / (w->dx * w->dx);
    const double complex off = -0.25 * I * s * h2;        /* coefficient of the neighbours in 1 + i s A / 2 */
    double complex *r = w->rhs, *c = w->c, *d = w->d;
    for (int k = 0; k < n; k++) {
        double complex left = k > 0 ? p[(k - 1) * stride] : 0.0, right = k < n - 1 ? p[(k + 1) * stride] : 0.0;
        double complex here = p[k * stride];
        double complex a_psi = h2 * here - 0.5 * h2 * (left + right) + 0.5 * v[k * stride] * here;
        r[k] = here - 0.5 * I * s * a_psi;
    }
    for (int k = 0; k < n; k++) {
        double complex diag = 1.0 + 0.5 * I * s * (h2 + 0.5 * v[k * stride]);
        if (k == 0) {
            c[k] = off / diag;
            d[k] = r[k] / diag;
        } else {
            double complex m = diag - off * c[k - 1];
            c[k] = off / m;
            d[k] = (r[k] - off * d[k - 1]) / m;
        }
    }
    for (int k = n - 1; k >= 0; k--) {
        double complex x = d[k] - (k < n - 1 ? c[k] * w->line[k + 1] : 0.0);
        w->line[k] = x;
    }
    for (int k = 0; k < n; k++) p[k * stride] = w->line[k];
}

static void sweep_x(Wave2 *w, double s)
{
    for (int j = 0; j < w->ny; j++) cayley_line(w, w->psi + j * w->nx, w->v + j * w->nx, w->nx, 1, s);
}

static void sweep_y(Wave2 *w, double s)
{
    for (int i = 0; i < w->nx; i++) cayley_line(w, w->psi + i, w->v + i, w->ny, w->nx, s);
}

void wave2_step(Wave2 *w)
{
    sweep_x(w, 0.5 * w->dt);
    sweep_y(w, w->dt);
    sweep_x(w, 0.5 * w->dt);
    if (w->damp)
        for (int k = 0; k < w->nx * w->ny; k++) w->psi[k] *= w->damp[k];
}

double wave2_norm(const Wave2 *w)
{
    double s = 0.0;
    for (int k = 0; k < w->nx * w->ny; k++) s += creal(w->psi[k] * conj(w->psi[k]));
    return s * w->dx * w->dx;
}

double wave2_mean(const Wave2 *w, int axis)
{
    double s = 0.0, m = 0.0;
    for (int j = 0; j < w->ny; j++)
        for (int i = 0; i < w->nx; i++) {
            double p = creal(w->psi[j * w->nx + i] * conj(w->psi[j * w->nx + i]));
            s += p;
            m += p * (axis ? wave2_y(w, j) : wave2_x(w, i));
        }
    return s > 0.0 ? m / s : 0.0;
}

double wave2_variance(const Wave2 *w, int axis)
{
    double mean = wave2_mean(w, axis), s = 0.0, m = 0.0;
    for (int j = 0; j < w->ny; j++)
        for (int i = 0; i < w->nx; i++) {
            double p = creal(w->psi[j * w->nx + i] * conj(w->psi[j * w->nx + i]));
            double q = (axis ? wave2_y(w, j) : wave2_x(w, i)) - mean;
            s += p;
            m += p * q * q;
        }
    return s > 0.0 ? m / s : 0.0;
}

double wave2_energy(const Wave2 *w)
{
    const double h2 = 1.0 / (w->dx * w->dx);
    double e = 0.0, s = 0.0;
    for (int j = 0; j < w->ny; j++)
        for (int i = 0; i < w->nx; i++) {
            int k = j * w->nx + i;
            double complex p = w->psi[k];
            double complex l = i > 0 ? w->psi[k - 1] : 0.0, r = i < w->nx - 1 ? w->psi[k + 1] : 0.0;
            double complex dn = j > 0 ? w->psi[k - w->nx] : 0.0, up = j < w->ny - 1 ? w->psi[k + w->nx] : 0.0;
            double complex hp = -0.5 * h2 * (l + r + dn + up - 4.0 * p) + w->v[k] * p;
            e += creal(conj(p) * hp);
            s += creal(conj(p) * p);
        }
    return s > 0.0 ? e / s : 0.0;
}

double wave2_prob_beyond(const Wave2 *w, double from)
{
    double s = 0.0;
    for (int j = 0; j < w->ny; j++)
        for (int i = 0; i < w->nx; i++)
            if (wave2_x(w, i) >= from - 1e-9) s += creal(w->psi[j * w->nx + i] * conj(w->psi[j * w->nx + i]));
    return s * w->dx * w->dx;
}

/* Fourier amplitude along y of column i at wavenumber ky: dx / sqrt(2 pi) sum_j psi e^{-i ky y_j}. */
static double complex column_amplitude(const Wave2 *w, int i, double ky)
{
    double complex a = 0.0, turn = cexp(-I * ky * w->dx), ph = cexp(-I * ky * w->y0);
    for (int j = 0; j < w->ny; j++) {
        a += w->psi[j * w->nx + i] * ph;
        ph *= turn;
    }
    return a * w->dx / sqrt(2.0 * PI);
}

void wave2_ky_spectrum(const Wave2 *w, double from, int count, const double *ky, double *out)
{
    for (int m = 0; m < count; m++) {
        double s = 0.0;
        for (int i = 0; i < w->nx; i++) {
            if (wave2_x(w, i) < from - 1e-9) continue;
            double complex a = column_amplitude(w, i, ky[m]);
            s += creal(a * conj(a));
        }
        out[m] = s * w->dx;
    }
}

void wave2_ky_cross(const Wave2 *a, const Wave2 *b, double from, int count, const double *ky, double *out)
{
    for (int m = 0; m < count; m++) {
        double s = 0.0;
        for (int i = 0; i < a->nx; i++) {
            if (wave2_x(a, i) < from - 1e-9) continue;
            s += creal(conj(column_amplitude(a, i, ky[m])) * column_amplitude(b, i, ky[m]));
        }
        out[m] = s * a->dx;
    }
}

/* Simpson's rule for the Gaussian-lit slit: integral of exp(-y^2/(4 sy^2) - i ky y) over [lo, hi]. */
static double complex lit_slit(double ky, double lo, double hi, double sy)
{
    enum { N = 400 };
    double h = (hi - lo) / N;
    double complex s = 0.0;
    for (int k = 0; k <= N; k++) {
        double y = lo + k * h, weight = k == 0 || k == N ? 1.0 : (k % 2 ? 4.0 : 2.0);
        double g = sy > 0.0 ? exp(-y * y / (4.0 * sy * sy)) : 1.0;
        s += weight * g * cexp(-I * ky * y);
    }
    return s * h / 3.0;
}

double slit_fraunhofer(double ky, double sep, double width, double sy, int open)
{
    double complex a = 0.0;
    if (open & 1) a += lit_slit(ky, 0.5 * sep - 0.5 * width, 0.5 * sep + 0.5 * width, sy);
    if (open & 2) a += lit_slit(ky, -0.5 * sep - 0.5 * width, -0.5 * sep + 0.5 * width, sy);
    return creal(a * conj(a)) / (2.0 * PI);
}

double slit_ideal(double ky, double sep, double width, int open)
{
    double u = 0.5 * ky * width, sinc = fabs(u) < 1e-12 ? 1.0 : sin(u) / u;
    double one = width * width * sinc * sinc / (2.0 * PI);
    if ((open & 3) == 3) return 4.0 * cos(0.5 * ky * sep) * cos(0.5 * ky * sep) * one;
    return (open & 3) ? one : 0.0;
}
