#ifndef WAVE_H
#define WAVE_H

#include <complex.h>

/* 1D time-dependent Schroedinger equation i dpsi/dt = -1/2 psi'' + V psi (hbar = m = 1),
 * Crank-Nicolson scheme (unitary, second order), hard walls at both ends. */
typedef struct {
    int n;
    double x0, dx, dt;
    double complex *psi;
    double *v;
    double complex *scratch_c, *scratch_d;   /* work arrays of the tridiagonal solver */
} Wave;

int wave_init(Wave *w, int n, double x0, double dx, double dt);
void wave_free(Wave *w);
double wave_x(const Wave *w, int j);
void wave_gaussian(Wave *w, double center, double sigma, double k0);   /* normalised */
/* Rectangular barrier: V = height for x in [left, left + width). Replaces the potential. */
void wave_set_barrier(Wave *w, double left, double width, double height);
void wave_step(Wave *w);
double wave_norm(const Wave *w);
double wave_mean_x(const Wave *w);
double wave_variance_x(const Wave *w);
double wave_energy(const Wave *w);
/* Probability to the right of x. */
double wave_prob_right(const Wave *w, double x);

/* Exact transmission of a rectangular barrier for a plane wave of wavenumber k. */
double barrier_transmission(double k, double height, double width);
/* The same averaged over the momentum distribution of a Gaussian packet of width sigma. */
double packet_transmission(double k0, double sigma, double height, double width);
/* Numerical experiment: send a packet at a barrier and return the transmitted probability. */
double simulate_transmission(double k0, double sigma, double height, double width);

/* ---- Two barriers (M2): resonant tunnelling ---- */

/* Two equal rectangular barriers of `height` and `width`, the second starting `gap` after the end of
 * the first: V = height on [left, left + width) and [left + width + gap, left + 2 width + gap).
 * Replaces the potential. */
void wave_set_double_barrier(Wave *w, double left, double width, double gap, double height);
/* Probability in [from, to). */
double wave_prob_between(const Wave *w, double from, double to);

/* Exact plane-wave transmission through `layers` constant steps by transfer matrices: step i spans
 * [edges[i], edges[i + 1]) at height heights[i]; V = 0 outside. Writes the reflection to *reflection
 * when it is not NULL (T + R = 1). */
double layers_transmission(double k, int layers, const double *edges, const double *heights, double *reflection);
/* The same for the two barriers of wave_set_double_barrier (the first one starting at x = 0). */
double double_barrier_transmission(double k, double height, double width, double gap);
/* The transmission peak in the energy window (elo, ehi): its energy and its full width at half
 * maximum. Returns 0 when a peak with T > 0.5 inside the window was found. */
int double_barrier_resonance(double height, double width, double gap, double elo, double ehi,
                             double *energy, double *fwhm);
/* Double-barrier transmission averaged over a Gaussian packet's momenta (as packet_transmission). */
double double_packet_transmission(double k0, double sigma, double height, double width, double gap);
/* Numerical experiment: a packet sent at the two barriers; returns the probability beyond them. */
double simulate_double_transmission(double k0, double sigma, double height, double width, double gap);
/* Numerical experiment: the lowest box state put between the barriers leaks out; returns the decay
 * rate of the probability left between them, fitted on t in [t1, t2] (theory: the width Gamma). */
double simulate_well_decay(double height, double width, double gap, double t1, double t2);

#endif
