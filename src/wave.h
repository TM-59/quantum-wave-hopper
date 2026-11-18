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
    double complex *scratch_c, *scratch_d; /* work arrays of the tridiagonal solver */
} Wave;

/* Arguments grouped by meaning, so that two numbers of the same type cannot change places. */
/* A grid of n points from x0 with spacing dx, and the time step dt. */
typedef struct {
    int n;
    double x0, dx, dt;
} Grid;
/* A Gaussian packet: centre, width sigma of |psi|^2 and mean wavenumber k0. */
typedef struct {
    double center, sigma, k0;
} Packet;
/* The incoming packet of the transmission formulas: mean wavenumber k0 and width sigma. */
typedef struct {
    double k0, sigma;
} Launch;
/* One rectangular barrier. */
typedef struct {
    double height, width;
} Barrier;
/* Two equal rectangular barriers with `gap` (the well) between them. */
typedef struct {
    double height, width, gap;
} DoubleBarrier;
/* A closed interval [from, to] of energies or times. */
typedef struct {
    double from, to;
} Span;
/* `count` layers: edges[0..count] and heights[0..count-1]. */
typedef struct {
    int count;
    const double *edges, *heights;
} Layers;
/* Where a resonance search writes the energy and the full width at half maximum. */
typedef struct {
    double *energy, *fwhm;
} ResonanceOut;

int wave_init(Wave *w, Grid grid);
void wave_free(Wave *w);
double wave_x(const Wave *w, int j);
void wave_gaussian(Wave *w, Packet packet); /* normalised */
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
double barrier_transmission(double k, Barrier bar);
/* The same averaged over the momentum distribution of a Gaussian packet of width sigma. */
double packet_transmission(Launch launch, Barrier bar);
/* Numerical experiment: send a packet at a barrier and return the transmitted probability. */
double simulate_transmission(Launch launch, Barrier bar);

/* ---- Two barriers (M2): resonant tunnelling ---- */

/* Two equal rectangular barriers of `height` and `width`, the second starting `gap` after the end
 * of the first: V = height on [left, left + width) and [left + width + gap, left + 2 width + gap).
 * Replaces the potential. */
void wave_set_double_barrier(Wave *w, double left, DoubleBarrier pair);
/* Probability in [from, to). */
double wave_prob_between(const Wave *w, double from, double to);

/* Exact plane-wave transmission through `layers` constant steps by transfer matrices: step i spans
 * [edges[i], edges[i + 1]) at height heights[i]; V = 0 outside. Writes the reflection to
 * *reflection when it is not NULL (T + R = 1). */
double layers_transmission(double k, Layers stack, double *reflection);
/* The same for the two barriers of wave_set_double_barrier (the first one starting at x = 0). */
double double_barrier_transmission(double k, DoubleBarrier pair);
/* The transmission peak in the energy window (elo, ehi): its energy and its full width at half
 * maximum. Returns 0 when a peak with T > 0.5 inside the window was found. */
int double_barrier_resonance(DoubleBarrier pair, Span search, ResonanceOut found);
/* Double-barrier transmission averaged over a Gaussian packet's momenta (as packet_transmission).
 */
double double_packet_transmission(Launch launch, DoubleBarrier pair);
/* Numerical experiment: a packet sent at the two barriers; returns the probability beyond them. */
double simulate_double_transmission(Launch launch, DoubleBarrier pair);
/* Numerical experiment: the lowest box state put between the barriers leaks out; returns the decay
 * rate of the probability left between them, fitted on t in [t1, t2] (theory: the width Gamma). */
double simulate_well_decay(DoubleBarrier pair, Span times);

#endif
