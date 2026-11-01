#include "observer.h"
#include "wave.h"
#include <complex.h>
#include <math.h>

#define GRID 3900
#define X0 -195.0
#define DX 0.1
#define DT 0.05
#define SIGMA 10.0
#define K0 1.0
#define START -100.0
#define WIDTH 2.0

static Wave wave;
static int ready, mode, steps;
static double left, height, half = -1.0;

double qw_half_height(void)
{
    if (half >= 0.0) return half;
    double lo = 0.0, hi = 2.0;   /* T falls monotonically with height for k0 = 1, width 2 */
    for (int i = 0; i < 60; i++) {
        double mid = 0.5 * (lo + hi);
        if (packet_transmission(K0, SIGMA, mid, WIDTH) > 0.5) lo = mid; else hi = mid;
    }
    return half = 0.5 * (lo + hi);
}

void qw_launch(void)
{
    if (!ready) return;
    wave_gaussian(&wave, START, SIGMA, K0);
    wave_set_barrier(&wave, left, WIDTH, height);
    steps = 0;
}

int qw_reset(int next_mode)
{
    if (!ready) {
        if (wave_init(&wave, GRID, X0, DX, DT)) return 1;
        ready = 1;
    }
    mode = next_mode == 1 || next_mode == 2 ? next_mode : 0;
    left = 0.0;
    height = mode == 2 ? qw_half_height() : 0.6;
    qw_launch();
    return 0;
}

void qw_controls(double next_left, double next_height)
{
    if (!ready || mode || !isfinite(next_left) || !isfinite(next_height)) return;
    left = fmax(-40.0, fmin(60.0, round(next_left / 2.0) * 2.0));
    height = fmax(0.0, fmin(1.5, next_height));
    wave_set_barrier(&wave, left, WIDTH, height);
}

void qw_step(int count)
{
    if (!ready || count < 0) return;
    if (count > 400) count = 400;
    for (int i = 0; i < count && steps < QW_STEPS_PER_RUN; i++, steps++) wave_step(&wave);
}

static double region(double from, double to)
{
    double s = 0.0;
    for (int j = 0; j < wave.n; j++) {
        double x = wave_x(&wave, j);
        if (x >= from - 1e-9 && x < to - 1e-9) s += creal(wave.psi[j] * conj(wave.psi[j]));
    }
    return s * wave.dx / wave_norm(&wave);
}

double qw_value(int field)
{
    if (!ready) return 0.0;
    switch (field) {
    case QW_TIME: return steps * DT;
    case QW_LEFT: return left;
    case QW_HEIGHT: return height;
    case QW_WIDTH: return WIDTH;
    case QW_NORM: return wave_norm(&wave);
    case QW_MEAN_X: return wave_mean_x(&wave);
    case QW_P_LEFT: return region(-1e9, left);
    case QW_P_INSIDE: return region(left, left + WIDTH);
    case QW_P_RIGHT: return wave_prob_right(&wave, left + WIDTH);
    case QW_ENERGY: return wave_energy(&wave);
    case QW_THEORY: return packet_transmission(K0, SIGMA, height, WIDTH);
    case QW_MODE: return mode;
    case QW_K0: return K0;
    case QW_SIGMA: return SIGMA;
    case QW_X_MIN: return X0;
    case QW_X_MAX: return X0 + (GRID - 1) * DX;
    case QW_BINS: return QW_BIN_COUNT;
    case QW_DONE: return steps >= QW_STEPS_PER_RUN;
    default: return 0.0;
    }
}

double qw_density(int bin)
{
    if (!ready || bin < 0 || bin >= QW_BIN_COUNT) return 0.0;
    int per = GRID / QW_BIN_COUNT;
    double s = 0.0;
    for (int j = bin * per; j < (bin + 1) * per; j++) s += creal(wave.psi[j] * conj(wave.psi[j]));
    return s / per;
}

double qw_real(int bin)
{
    if (!ready || bin < 0 || bin >= QW_BIN_COUNT) return 0.0;
    int per = GRID / QW_BIN_COUNT;
    return creal(wave.psi[bin * per + per / 2]);
}
