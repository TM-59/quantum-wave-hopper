#include "resonance.h"
#include "wave.h"

#include <complex.h>
#include <math.h>

#define X0 -400.0
#define X1 700.0
#define DX 0.1
#define DT 0.05
#define HEIGHT 1.0
#define WIDTH 1.0
#define GAP 4.0
#define SPAN (2.0 * WIDTH + GAP)
#define SIGMA 20.0
#define START -140.0
#define DECAY_T 200.0
#define E_LO 0.02
#define E_HI 0.80

static Wave wave;
static int ready, mode, steps, finished;
static double energy = 0.30, tmax, t0, res[2][2] = {{-1.0, 0.0}, {-1.0, 0.0}}, packet_t = -1.0;
static double curve[RS_CURVE_COUNT], curve_packet[RS_CURVE_COUNT];
static int curve_ready, curve_packet_ready;

static void resonances(void)
{
    if (res[0][0] >= 0.0) return;
    double e = 0.0, g = 0.0;
    if (double_barrier_resonance(HEIGHT, WIDTH, GAP, 0.01, HEIGHT, &e, &g) == 0) { res[0][0] = e; res[0][1] = g; }
    if (double_barrier_resonance(HEIGHT, WIDTH, GAP, res[0][0] + 3.0 * res[0][1], HEIGHT, &e, &g) == 0) { res[1][0] = e; res[1][1] = g; }
}

/* Run length: the slow side of the packet has passed and what lingers in the well has had 400 time
 * units to leave, but the fast side never reaches the right wall. */
static void plan(void)
{
    if (mode == 1) { tmax = t0 = DECAY_T; return; }
    double k0 = sqrt(2.0 * energy), sk = 0.5 / SIGMA, slow = fmax(k0 - 3.0 * sk, 0.25 * k0), fast = k0 + 3.0 * sk;
    t0 = (fabs(START) + SPAN + 3.0 * SIGMA) / slow;
    tmax = fmin(t0 + 400.0, (X1 - SPAN - 6.0 * SIGMA) / fast);
}

void rs_launch(void)
{
    if (!ready) return;
    wave_set_double_barrier(&wave, 0.0, WIDTH, GAP, HEIGHT);
    if (mode == 1) {
        for (int j = 0; j < wave.n; j++) {
            double x = wave_x(&wave, j);
            wave.psi[j] = x > WIDTH && x < WIDTH + GAP ? sin(3.14159265358979323846 * (x - WIDTH) / GAP) : 0.0;
        }
        double norm = sqrt(wave_norm(&wave));
        for (int j = 0; j < wave.n; j++) wave.psi[j] /= norm;
    } else {
        wave_gaussian(&wave, START, SIGMA, sqrt(2.0 * energy));
    }
    steps = 0;
    finished = 0;
    plan();
}

int rs_reset(int next_mode)
{
    if (!ready) {
        if (wave_init(&wave, (int)((X1 - X0) / DX + 0.5), X0, DX, DT)) return 1;
        ready = 1;
    }
    resonances();
    mode = next_mode == 1 ? 1 : 0;
    rs_launch();
    return 0;
}

void rs_energy(double next)
{
    if (!ready || !isfinite(next)) return;
    next = fmax(RS_E_MIN, fmin(RS_E_MAX, next));
    if (next != energy) packet_t = -1.0;
    energy = next;
    mode = 0;
    rs_launch();
}

void rs_step(int count)
{
    if (!ready || count < 0) return;
    if (count > 400) count = 400;
    for (int i = 0; i < count && !finished; i++) {
        wave_step(&wave);
        steps++;
        double t = steps * DT;
        if (t >= tmax - 1e-9 || (mode == 0 && t >= t0 && wave_prob_between(&wave, WIDTH, WIDTH + GAP) < 1e-5)) finished = 1;
    }
}

double rs_value(int field)
{
    if (!ready) return 0.0;
    switch (field) {
    case RS_TIME: return steps * DT;
    case RS_MODE: return mode;
    case RS_ENERGY: return energy;
    case RS_K0: return sqrt(2.0 * energy);
    case RS_SIGMA: return SIGMA;
    case RS_HEIGHT: return HEIGHT;
    case RS_WIDTH: return WIDTH;
    case RS_GAP: return GAP;
    case RS_NORM: return wave_norm(&wave);
    case RS_P_LEFT: return wave_prob_between(&wave, X0 - 1.0, 0.0);
    case RS_P_WELL: return wave_prob_between(&wave, WIDTH, WIDTH + GAP);
    case RS_P_RIGHT: return wave_prob_right(&wave, SPAN);
    case RS_T_PLANE: return double_barrier_transmission(sqrt(2.0 * energy), HEIGHT, WIDTH, GAP);
    case RS_T_PACKET:
        if (packet_t < 0.0) packet_t = double_packet_transmission(sqrt(2.0 * energy), SIGMA, HEIGHT, WIDTH, GAP);
        return packet_t;
    case RS_E1: return res[0][0];
    case RS_G1: return res[0][1];
    case RS_E2: return res[1][0];
    case RS_G2: return res[1][1];
    case RS_DONE: return finished;
    case RS_T_MAX: return tmax;
    case RS_VIEW_MIN: return -200.0;
    case RS_VIEW_MAX: return 200.0;
    case RS_BINS: return RS_BIN_COUNT;
    case RS_ZOOM_MIN: return -3.0;
    case RS_ZOOM_MAX: return 9.0;
    case RS_ZOOM_SITES: return RS_ZOOM_COUNT;
    case RS_CURVE_E0: return E_LO;
    case RS_CURVE_E1: return E_HI;
    case RS_CURVE_POINTS: return RS_CURVE_COUNT;
    default: return 0.0;
    }
}

static int site_of(double x) { return (int)lround((x - X0) / DX); }

double rs_density(int bin)
{
    if (!ready || bin < 0 || bin >= RS_BIN_COUNT) return 0.0;
    int first = site_of(-200.0) + bin * 10;
    double s = 0.0;
    for (int j = first; j < first + 10; j++) s += creal(wave.psi[j] * conj(wave.psi[j]));
    return s / 10.0;
}

double rs_zoom(int site)
{
    if (!ready || site < 0 || site >= RS_ZOOM_COUNT) return 0.0;
    int j = site_of(-3.0) + site;
    return creal(wave.psi[j] * conj(wave.psi[j]));
}

static double curve_energy(int i) { return E_LO + (E_HI - E_LO) * i / (RS_CURVE_COUNT - 1); }

double rs_curve(int point)
{
    if (point < 0 || point >= RS_CURVE_COUNT) return 0.0;
    if (!curve_ready) {
        for (int i = 0; i < RS_CURVE_COUNT; i++) curve[i] = double_barrier_transmission(sqrt(2.0 * curve_energy(i)), HEIGHT, WIDTH, GAP);
        curve_ready = 1;
    }
    return curve[point];
}

double rs_curve_packet(int point)
{
    if (point < 0 || point >= RS_CURVE_COUNT) return 0.0;
    if (!curve_packet_ready) {
        for (int i = 0; i < RS_CURVE_COUNT; i++)
            curve_packet[i] = double_packet_transmission(sqrt(2.0 * curve_energy(i)), SIGMA, HEIGHT, WIDTH, GAP);
        curve_packet_ready = 1;
    }
    return curve_packet[point];
}
