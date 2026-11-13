#ifndef SLITS_H
#define SLITS_H

/* Browser adapter of the double-slit experiment (M3): the 2D solver of wave2d.h on a 256 x 256 grid
 * (dx 0.2, x and y in [-25.6, 25.6)), read through plain doubles and one RGBA image.
 * A wall 0.8 thick (V = 60) at x = 0 with two slits; a packet (sigma_x 3, sigma_y 6) starts at x = -12.
 * Modes: 0 both slits open, 1 slit A (upper, y = +sep/2) only, 2 slit B (lower) only,
 * 3 which-path watched: the two one-slit runs side by side, combined with a random phase on slit B. */
enum {
    SL_TIME, SL_MODE, SL_LAMBDA, SL_K, SL_SEP, SL_WIDTH, SL_SCRAMBLE, SL_COHERENCE, SL_NORM, SL_P_BEYOND,
    SL_DONE, SL_T_MAX, SL_STEPS, SL_NX, SL_NY, SL_X0, SL_Y0, SL_DX, SL_WALL_X, SL_WALL_T, SL_SCREEN_L,
    SL_SCREEN_HALF, SL_SCREEN_BINS, SL_SPACING, SL_DKY, SL_VISIBILITY, SL_VISIBILITY_THEORY, SL_SPACING_MEASURED,
    SL_MEASURED_AT, SL_SEED, SL_FIELDS
};
#define SL_BINS 200
#define SL_LAMBDA_MIN 2.0
#define SL_LAMBDA_MAX 4.0
#define SL_SEP_MIN 4.0
#define SL_SEP_MAX 14.0
#define SL_WIDTH_MIN 1.0
#define SL_WIDTH_MAX 3.8

int sl_reset(int mode);
/* Wavelength (snapped to 0.1), slit separation (to 0.4) and slit width (to 1.0 + 0.4 n), clamped so that
 * at least 1.0 of wall stays between the slits; relaunches the packet. */
void sl_controls(double lambda, double sep, double width);
/* Spread of the random phase on slit B in mode 3: uniform on [-phi, phi], phi in [0, pi]. */
void sl_scramble(double phi);
void sl_seed(double seed);       /* seed of the particle counter (an integer below 2^31) */
void sl_launch(void);
void sl_step(int steps);         /* at most 50 per call; stops at the end of the run */
/* Recomputes the far-field screen from the current wave (the expensive part: call it every few frames). */
void sl_measure(void);
double sl_value(int field);
/* Phase-coloured density as RGBA, row 0 at the top (largest y), column 0 at the left (smallest x).
 * Mode 3 shows intensity only: the two runs have no common phase. Returns a pointer into memory. */
unsigned char *sl_image(void);
double sl_screen(int bin);        /* measured intensity on the screen at distance L, area 1 over the screen */
double sl_screen_theory(int bin); /* Fraunhofer of the Gaussian-lit slits, the same normalisation */
double sl_screen_ideal(int bin);  /* the textbook cos^2 sinc^2 (uniform light), the same normalisation */
/* Screen position of particle n, drawn from the measured screen intensity by the counter (seed, n). */
double sl_particle(int n);

#endif
