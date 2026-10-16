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

#endif
