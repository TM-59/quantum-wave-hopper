#ifndef WAVE2D_H
#define WAVE2D_H

#include <complex.h>

/* 2D time-dependent Schroedinger equation i dpsi/dt = -1/2 (psi_xx + psi_yy) + V psi (hbar = m = 1) on a
 * square grid (dy = dx), hard walls on all four sides. One step is the symmetric product
 *   C_x(dt/2) C_y(dt) C_x(dt/2),   C_a(s) = (1 + i s A_a / 2)^-1 (1 - i s A_a / 2),
 *   A_x = -1/2 d^2/dx^2 + V/2,  A_y = -1/2 d^2/dy^2 + V/2,
 * an alternating-direction Crank-Nicolson scheme: every factor is a Cayley transform of a Hermitian
 * matrix (exactly unitary) and solves independent tridiagonal systems along one axis; the symmetric
 * (Strang) order makes the step second order in dt. Separate from the 1D solver of wave.h. */
typedef struct {
    int nx, ny;
    double x0, y0, dx, dt;
    double complex *psi;          /* psi[j * nx + i] at (x0 + i dx, y0 + j dx) */
    double *v;
    double *damp;                 /* per-step absorbing factor in (0, 1], NULL when there is no absorber */
    double complex *line, *rhs, *c, *d;   /* work arrays of one line */
} Wave2;

int wave2_init(Wave2 *w, int nx, int ny, double x0, double y0, double dx, double dt);
void wave2_free(Wave2 *w);
double wave2_x(const Wave2 *w, int i);
double wave2_y(const Wave2 *w, int j);
/* Normalised Gaussian packet centred at (cx, cy) with widths (sx, sy) of |psi|^2 and wavevector (kx, ky). */
void wave2_gaussian(Wave2 *w, double cx, double cy, double sx, double sy, double kx, double ky);
/* A wall V = height for x in [wall_x, wall_x + thickness) with up to two slits of `width` centred at
 * y = +sep/2 (slit A) and y = -sep/2 (slit B); `open` bit 1 opens A, bit 2 opens B. A grid point is open
 * when the cell [y - dx/2, y + dx/2) lies more than half inside a slit. Replaces the potential. */
void wave2_set_slits(Wave2 *w, double wall_x, double thickness, double height, double sep, double width, int open);
/* Absorbing layers of `layer` length on all four edges: after every step psi is multiplied by
 * exp(-strength s^2 dt), s the depth into the layer from 0 to 1. strength <= 0 removes them. */
int wave2_absorber(Wave2 *w, double layer, double strength);
void wave2_step(Wave2 *w);
double wave2_norm(const Wave2 *w);
/* Mean and variance of x (axis 0) or y (axis 1) under |psi|^2 / norm. */
double wave2_mean(const Wave2 *w, int axis);
double wave2_variance(const Wave2 *w, int axis);
double wave2_energy(const Wave2 *w);
/* Probability with x >= from. */
double wave2_prob_beyond(const Wave2 *w, double from);
/* Transverse momentum distribution of the part with x >= from:
 * out[m] = sum over those columns i of dx |dx / sqrt(2 pi) sum_j psi(i, j) e^{-i ky[m] y_j}|^2,
 * so that its integral over ky is that part's probability (Parseval). */
void wave2_ky_spectrum(const Wave2 *w, double from, int count, const double *ky, double *out);
/* The same for a second wave b of the same grid: the real part of the cross term
 * sum dx Re(conj(A(ky)) B(ky)), for interference between two runs. */
void wave2_ky_cross(const Wave2 *a, const Wave2 *b, double from, int count, const double *ky, double *out);

/* Fraunhofer diffraction of slits lit by the transverse Gaussian amplitude exp(-y^2 / (4 sy^2)):
 * |integral over the open slits of exp(-y^2/(4 sy^2) - i ky y) dy|^2 / (2 pi). sy <= 0 means uniform light.
 * open as in wave2_set_slits. */
double slit_fraunhofer(double ky, double sep, double width, double sy, int open);
/* The textbook closed form for uniform light: single slit width^2 sinc^2(ky width / 2) / (2 pi);
 * both open adds the factor 4 cos^2(ky sep / 2). */
double slit_ideal(double ky, double sep, double width, int open);

#endif
