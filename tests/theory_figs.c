/* Regenerates every plotted figure of docs/theory and the table of measured values
 * (docs/theory/figures/values.md). Run with `make theory-figs`. Each figure is produced by the
 * same solver (src/wave.c) and browser adapter (src/observer.c) the program uses; nothing here
 * changes them. */
#include "../src/observer.h"
#include "../src/svg.h"
#include "../src/wave.h"

#include <complex.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define PI 3.14159265358979323846
#define MAXN 4096

static char dir[512];
static FILE *values;
static float xs[MAXN], ya[MAXN], yb[MAXN], yc[MAXN];

static const char *path(const char *name)
{
    static char buf[640];
    snprintf(buf, sizeof buf, "%s/%s", dir, name);
    return buf;
}

static void plot(const char *name, const char *title, const char *xl, const char *yl, int count, const Series *s, int ns)
{
    if (svg_plot(path(name), title, xl, yl, xs, count, s, ns) != 0) {
        fprintf(stderr, "cannot write %s\n", name);
        exit(1);
    }
}

/* One row of the measured-values table: id | what | measured | theory | error (relative; absolute when theory is 0). */
static void value(const char *id, const char *what, double measured, double theory)
{
    double err = theory != 0.0 ? fabs(measured / theory - 1.0) : fabs(measured - theory);
    fprintf(values, "| `%s` | %s | %.8g | %.8g | %.2e |\n", id, what, measured, theory, err);
}

static void must(int failed)
{
    if (failed) { fprintf(stderr, "out of memory\n"); exit(1); }
}

/* S01: norm and energy of the game's own experiment, step by step. */
static void fig_conservation(void)
{
    Wave w;
    must(wave_init(&w, 3900, -195.0, 0.1, 0.05));
    wave_gaussian(&w, -100.0, 10.0, 1.0);
    wave_set_barrier(&w, 0.0, 2.0, 0.6);
    double e0 = wave_energy(&w), worst_n = 0.0, worst_e = 0.0;
    int n = 0;
    for (int k = 0; k <= 3300; k++) {
        double dn = wave_norm(&w) - 1.0, de = wave_energy(&w) / e0 - 1.0;
        worst_n = fmax(worst_n, fabs(dn));
        worst_e = fmax(worst_e, fabs(de));
        if (k % 15 == 0) { xs[n] = (float)(k * 0.05); ya[n] = (float)(dn * 1e12); yb[n] = (float)(de * 1e12); n++; }
        if (k < 3300) wave_step(&w);
    }
    value("norm-drift", "largest departure of the total probability from 1 over the 3300 steps of the standard experiment (theory 0)", worst_n, 0.0);
    value("energy-drift", "largest relative change of the mean energy over the same run, barrier included (theory 0)", worst_e, 0.0);
    value("energy-packet", "mean energy of the packet k0 = 1, sigma = 10 on the grid (continuum k0^2 / 2 + 1 / (8 sigma^2))", e0, 0.5 + 1.0 / 800.0);
    Series s[] = {{"norm - 1 (x 1e12)", ya, "#1f77b4"}, {"E / E0 - 1 (x 1e12)", yb, "#d62728"}};
    plot("conservation.svg", "Total probability and mean energy through a tunnelling run", "t", "departure x 1e12", n, s, 2);
    wave_free(&w);
}

/* S02: a free packet moves at its group velocity and spreads. */
static void fig_packet(void)
{
    Wave w;
    double sigma = 2.0, k0 = 1.5;
    must(wave_init(&w, 4000, -100.0, 0.05, 0.01));
    wave_gaussian(&w, -40.0, sigma, k0);
    int n = 0;
    double mean_end = 0.0, width_end = 0.0;
    for (int k = 0; k <= 3000; k++) {
        double t = k * 0.01;
        if (k % 15 == 0) {
            xs[n] = (float)t;
            ya[n] = (float)sqrt(wave_variance_x(&w));
            yb[n] = (float)(sigma * sqrt(1.0 + t * t / (4.0 * pow(sigma, 4))));
            yc[n] = (float)((wave_mean_x(&w) + 40.0) / 10.0);
            n++;
        }
        if (k == 3000) { mean_end = wave_mean_x(&w); width_end = sqrt(wave_variance_x(&w)); }
        if (k < 3000) wave_step(&w);
    }
    value("packet-centre", "centre of a free packet (k0 = 1.5, sigma = 2) after t = 30, started at -40 (theory -40 + k0 t)", mean_end, -40.0 + 1.5 * 30.0);
    value("packet-width", "its width after t = 30 (theory sigma sqrt(1 + (t / 2 sigma^2)^2))", width_end, sigma * sqrt(1.0 + 900.0 / 64.0));
    Wave g;
    must(wave_init(&g, 4000, -100.0, 0.05, 0.01));
    wave_gaussian(&g, 0.0, 2.0, 1.5);
    value("packet-width-0", "width of the packet as launched (theory sigma = 2)", sqrt(wave_variance_x(&g)), 2.0);
    value("packet-norm-0", "wave_norm right after wave_gaussian (theory 1)", wave_norm(&g), 1.0);
    value("packet-energy", "its mean energy (theory k0^2 / 2 + 1 / (8 sigma^2) = 1.125 + 0.03125)", wave_energy(&g), 1.15625);
    wave_free(&g);
    Series s[] = {{"width", ya, "#1f77b4"}, {"theory", yb, "#d62728"}, {"distance / 10", yc, "#888888"}};
    plot("packet.svg", "A free packet: spreading width and distance covered", "t", "width, distance / 10", n, s, 3);
    wave_free(&w);
}

/* Speed of a free packet's centre on the game's grid. */
static double centre_speed(double k0, double dx, double dt)
{
    Wave w;
    int n = (int)(400.0 / dx);
    must(wave_init(&w, n, -200.0, dx, dt));
    wave_gaussian(&w, -100.0, 10.0, k0);
    int steps = (int)(60.0 / k0 / dt);
    double x0 = wave_mean_x(&w);
    for (int k = 0; k < steps; k++) wave_step(&w);
    double v = (wave_mean_x(&w) - x0) / (steps * dt);
    wave_free(&w);
    return v;
}

/* S03: the grid's own dispersion relation. */
static void fig_dispersion(void)
{
    double dx = 0.1, dt = 0.05;
    int n = 0;
    for (double k = 0.25; k <= 4.0001; k += 0.25, n++) {
        double e = (1.0 - cos(k * dx)) / (dx * dx), q = 0.5 * e * dt;
        xs[n] = (float)k;
        ya[n] = (float)(centre_speed(k, dx, dt) / k);
        yb[n] = (float)(sin(k * dx) / dx / (1.0 + q * q) / k);
        yc[n] = 1.0f;
    }
    double e1 = (1.0 - cos(dx)) / (dx * dx), q1 = 0.5 * e1 * dt;
    value("group-speed-k1", "speed of the packet centre at k0 = 1 on the game's grid (dx 0.1, dt 0.05) (theory of the scheme: sin(k dx) / dx / (1 + (E dt / 2)^2))", centre_speed(1.0, dx, dt), sin(dx) / dx / (1.0 + q1 * q1));
    value("group-speed-k1-ideal", "the same against the continuum value k0 = 1", centre_speed(1.0, dx, dt), 1.0);
    double e3 = (1.0 - cos(3.0 * dx)) / (dx * dx), q3 = 0.5 * e3 * dt;
    value("group-speed-k3", "speed at k0 = 3 (theory of the scheme)", centre_speed(3.0, dx, dt), sin(3.0 * dx) / dx / (1.0 + q3 * q3));
    value("group-speed-k3-ideal", "the same against the continuum value 3", centre_speed(3.0, dx, dt), 3.0);
    Series s[] = {{"measured / k", ya, "#1f77b4"}, {"scheme / k", yb, "#d62728"}, {"continuum", yc, "#888888"}};
    plot("dispersion.svg", "Group velocity on the grid, relative to the continuum value k", "k0", "speed / k0", n, s, 3);
}

/* S04: the continuity equation, exactly as the scheme satisfies it. */
static void fig_current(void)
{
    Wave w;
    must(wave_init(&w, 3900, -195.0, 0.1, 0.05));
    wave_gaussian(&w, -100.0, 10.0, 1.0);
    wave_set_barrier(&w, 0.0, 2.0, 0.6);
    int site = (int)((2.0 - w.x0) / w.dx + 0.5), n = 0;   /* first site with x >= 2: the right edge of the barrier */
    double complex *old = malloc(sizeof *old * (size_t)w.n);
    must(!old);
    double worst = 0.0, total = 0.0, peak = 0.0;
    for (int k = 0; k < 3300; k++) {
        memcpy(old, w.psi, sizeof *old * (size_t)w.n);
        double before = 0.0, after = 0.0;
        for (int j = site; j < w.n; j++) before += creal(old[j] * conj(old[j])) * w.dx;
        wave_step(&w);
        for (int j = site; j < w.n; j++) after += creal(w.psi[j] * conj(w.psi[j])) * w.dx;
        double complex a = 0.5 * (old[site - 1] + w.psi[site - 1]), b = 0.5 * (old[site] + w.psi[site]);
        double current = cimag(conj(a) * b) / w.dx;       /* probability per unit time crossing from site-1 to site */
        worst = fmax(worst, fabs((after - before) / w.dt - current));
        total += current * w.dt;
        peak = fmax(peak, current);
        if (k % 15 == 0) { xs[n] = (float)((k + 0.5) * 0.05); ya[n] = (float)((after - before) / w.dt * 1000.0); yb[n] = (float)(current * 1000.0); n++; }
    }
    value("continuity", "largest difference between the rate of change of the probability beyond the barrier and the current through its edge, over 3300 steps (theory 0)", worst, 0.0);
    value("current-integral", "that current added up over the run against the probability found beyond the barrier at the end", total, wave_prob_right(&w, 2.0));
    value("current-peak", "largest current through the edge [probability per unit time]", peak, 0.0);
    free(old);
    Series s[] = {{"dP/dt (x 1000)", ya, "#1f77b4"}, {"current (x 1000)", yb, "#d62728"}};
    plot("current.svg", "Probability arriving beyond the barrier: rate of change against current", "t", "per unit time x 1000", n, s, 2);
    wave_free(&w);
}

/* S05: the plane-wave transmission of the rectangular barrier. */
static void fig_barrier(void)
{
    int n = 0;
    double height = 0.6, width = 2.0;
    for (double e = 0.01; e <= 3.0001; e += 0.01, n++) {
        double k = sqrt(2.0 * e);
        xs[n] = (float)(e / height);
        ya[n] = (float)barrier_transmission(k, height, width);
        yb[n] = (float)(e > height ? 1.0 : 0.0);   /* a classical particle */
    }
    value("barrier-none", "barrier_transmission with no barrier (theory 1)", barrier_transmission(2.0, 0.0, 3.0), 1.0);
    value("barrier-game", "plane wave k = 1 at the standard barrier V = 0.6, width 2: E / V = 0.83 (theory: the tunnelling formula)", barrier_transmission(1.0, 0.6, 2.0),
          1.0 / (1.0 + 0.36 * pow(sinh(sqrt(0.2) * 2.0), 2) / (4.0 * 0.5 * 0.1)));
    value("barrier-at-top", "exactly at E = V = 0.6 (theory 1 / (1 + V w^2 / 2))", barrier_transmission(sqrt(1.2), 0.6, 2.0), 1.0 / (1.0 + 0.5 * 0.6 * 4.0));
    double q = PI / width, eres = height + 0.5 * q * q;
    value("barrier-resonance", "at the first resonance above the barrier, q w = pi (theory 1)", barrier_transmission(sqrt(2.0 * eres), height, width), 1.0);
    value("barrier-continuity", "jump of the formula across E = V, between E = V (1 - 1e-9) and E = V (1 + 1e-9) (theory 0)",
          barrier_transmission(sqrt(1.2 * (1.0 + 1e-9)), 0.6, 2.0) - barrier_transmission(sqrt(1.2 * (1.0 - 1e-9)), 0.6, 2.0), 0.0);
    value("barrier-thick", "a tall, thick barrier: k = 0.5, V = 5, width 4 (theory about 16 E (V - E) / V^2 exp(-2 kappa w))", barrier_transmission(0.5, 5.0, 4.0),
          16.0 * 0.125 * 4.875 / 25.0 * exp(-2.0 * sqrt(9.75) * 4.0));
    Series s[] = {{"quantum T", ya, "#1f77b4"}, {"classical", yb, "#888888"}};
    plot("barrier.svg", "Transmission of a plane wave (barrier V = 0.6, width 2)", "E / V", "T", n, s, 2);
}

/* S06: the experiment against the packet-averaged formula. */
static void fig_experiment(void)
{
    int n = 0;
    double worst = 0.0;
    for (double h = 0.0; h <= 1.5001; h += 0.1, n++) {
        double sim = simulate_transmission(1.0, 10.0, h, 2.0), th = packet_transmission(1.0, 10.0, h, 2.0);
        xs[n] = (float)h;
        ya[n] = (float)sim;
        yb[n] = (float)th;
        yc[n] = (float)barrier_transmission(1.0, h, 2.0);
        if (th > 0.01) worst = fmax(worst, fabs(sim / th - 1.0));
    }
    value("tunnel-sim", "transmitted probability of the simulated packet at V = 0.6 (theory: the packet-averaged formula)", simulate_transmission(1.0, 10.0, 0.6, 2.0), packet_transmission(1.0, 10.0, 0.6, 2.0));
    value("tunnel-plane", "the packet-averaged formula at V = 0.6 against the single plane wave k = 1", packet_transmission(1.0, 10.0, 0.6, 2.0), barrier_transmission(1.0, 0.6, 2.0));
    value("tunnel-worst", "largest relative gap between simulation and packet formula over 16 heights, where T is above 0.01", worst, 0.0);
    value("tunnel-free", "simulated transmission with no barrier (theory 1)", simulate_transmission(1.0, 10.0, 0.0, 2.0), 1.0);
    Series s[] = {{"simulated packet", ya, "#1f77b4"}, {"packet formula", yb, "#d62728"}, {"plane wave k = 1", yc, "#888888"}};
    plot("experiment.svg", "Transmitted probability against barrier height (k0 = 1, width 2)", "barrier height V", "T", n, s, 3);
}

/* S07: Crank-Nicolson keeps the norm; the explicit scheme does not. */
static void fig_unitarity(void)
{
    Wave w, e;
    must(wave_init(&w, 800, -40.0, 0.1, 0.05));
    must(wave_init(&e, 800, -40.0, 0.1, 0.05));
    wave_gaussian(&w, 0.0, 3.0, 1.0);
    wave_gaussian(&e, 0.0, 3.0, 1.0);
    double complex *tmp = malloc(sizeof *tmp * 800);
    must(!tmp);
    int n = 0;
    double inv = 1.0 / (e.dx * e.dx);
    for (int k = 0; k <= 60; k++) {
        xs[n] = (float)k;
        ya[n] = (float)log10(wave_norm(&w));
        yb[n] = (float)log10(wave_norm(&e));
        n++;
        wave_step(&w);
        for (int j = 0; j < e.n; j++) {   /* explicit Euler: psi - i dt H psi */
            double complex l = j > 0 ? e.psi[j - 1] : 0.0, r = j < e.n - 1 ? e.psi[j + 1] : 0.0;
            tmp[j] = e.psi[j] - I * e.dt * (inv * e.psi[j] - 0.5 * inv * (l + r));
        }
        memcpy(e.psi, tmp, sizeof *tmp * 800);
    }
    value("cn-norm-60", "norm after 60 Crank-Nicolson steps (theory 1)", wave_norm(&w), 1.0);
    value("euler-norm-60", "log10 of the norm after 60 explicit Euler steps on the same grid (unstable: it grows without bound)", log10(wave_norm(&e)), 0.0);
    /* A step 200 times longer is still norm-preserving. */
    Wave big;
    must(wave_init(&big, 800, -40.0, 0.1, 10.0));
    wave_gaussian(&big, 0.0, 3.0, 1.0);
    for (int k = 0; k < 50; k++) wave_step(&big);
    value("cn-norm-big-step", "norm after 50 Crank-Nicolson steps of dt = 10, 200 times the game's (theory 1: unitary for any step)", wave_norm(&big), 1.0);
    wave_free(&big);
    free(tmp);
    Series s[] = {{"Crank-Nicolson", ya, "#1f77b4"}, {"explicit Euler", yb, "#d62728"}};
    plot("unitarity.svg", "Total probability over 60 steps (log scale)", "step", "log10 of the norm", n, s, 2);
    wave_free(&w);
    wave_free(&e);
}

/* S08: the tridiagonal solve really solves the system. */
static void val_thomas(void)
{
    Wave w;
    must(wave_init(&w, 3900, -195.0, 0.1, 0.05));
    wave_gaussian(&w, -3.0, 10.0, 1.0);
    wave_set_barrier(&w, 0.0, 2.0, 0.6);
    int n = w.n;
    double complex *old = malloc(sizeof *old * (size_t)n);
    must(!old);
    memcpy(old, w.psi, sizeof *old * (size_t)n);
    wave_step(&w);
    double inv = 1.0 / (w.dx * w.dx), r = w.dt / 2.0, worst = 0.0;
    for (int j = 0; j < n; j++) {
        double complex lo = j > 0 ? old[j - 1] : 0.0, ro = j < n - 1 ? old[j + 1] : 0.0;
        double complex ln = j > 0 ? w.psi[j - 1] : 0.0, rn = j < n - 1 ? w.psi[j + 1] : 0.0;
        double complex h_old = (inv + w.v[j]) * old[j] - 0.5 * inv * (lo + ro);
        double complex h_new = (inv + w.v[j]) * w.psi[j] - 0.5 * inv * (ln + rn);
        worst = fmax(worst, cabs((w.psi[j] + I * r * h_new) - (old[j] - I * r * h_old)));
    }
    value("thomas-residual", "largest residual of (1 + i dt H / 2) psi_new = (1 - i dt H / 2) psi_old over the 3900 sites after one step (theory 0)", worst, 0.0);
    /* Reversing time: a step with -dt undoes a step with +dt. */
    w.dt = -w.dt;
    wave_step(&w);
    double back = 0.0;
    for (int j = 0; j < n; j++) back = fmax(back, cabs(w.psi[j] - old[j]));
    value("step-reverse", "largest difference from the start after one step forward and one with dt reversed (theory 0)", back, 0.0);
    free(old);
    wave_free(&w);
}

/* Distance in the L2 sense between two wave functions on the same grid. */
static double l2(const Wave *a, const Wave *b)
{
    double s = 0.0;
    for (int j = 0; j < a->n; j++) s += pow(cabs(a->psi[j] - b->psi[j]), 2);
    return sqrt(s * a->dx);
}

/* S09: second order in the time step. */
static void fig_order(void)
{
    double dts[6] = {0.4, 0.2, 0.1, 0.05, 0.025, 0.0125}, errs[6];
    Wave ref;
    must(wave_init(&ref, 2000, -100.0, 0.1, 0.0015625));
    wave_gaussian(&ref, -30.0, 4.0, 1.0);
    wave_set_barrier(&ref, 0.0, 2.0, 0.6);
    for (int k = 0; k < 25600; k++) wave_step(&ref);   /* t = 40 */
    for (int i = 0; i < 6; i++) {
        Wave w;
        must(wave_init(&w, 2000, -100.0, 0.1, dts[i]));
        wave_gaussian(&w, -30.0, 4.0, 1.0);
        wave_set_barrier(&w, 0.0, 2.0, 0.6);
        int steps = (int)(40.0 / dts[i] + 0.5);
        for (int k = 0; k < steps; k++) wave_step(&w);
        errs[i] = l2(&w, &ref);
        xs[i] = (float)log10(dts[i]);
        ya[i] = (float)log10(errs[i]);
        yb[i] = (float)(log10(errs[0]) + 2.0 * (log10(dts[i]) - log10(dts[0])));
        wave_free(&w);
    }
    value("order-time", "slope of log(error) against log(dt) between dt = 0.1 and 0.025, same grid, reference dt = 0.0016 (theory 2)", log(errs[2] / errs[4]) / log(4.0), 2.0);
    value("error-game-dt", "L2 distance from the reference at the game's dt = 0.05 after t = 40 (a wave function of norm 1)", errs[3], 0.0);
    Series s[] = {{"error", ya, "#1f77b4"}, {"slope 2", yb, "#d62728"}};
    plot("order.svg", "Error against time step at t = 40 (log-log)", "log10 dt", "log10 of the L2 error", 6, s, 2);
    wave_free(&ref);
}

/* S10: a stationary state only turns its phase. */
static void fig_stationary(void)
{
    Wave w;
    must(wave_init(&w, 800, -20.0, 0.05, 0.01));
    for (int j = 0; j < w.n; j++) {
        double x = wave_x(&w, j);
        w.v[j] = 0.5 * x * x;
        w.psi[j] = exp(-x * x / 2.0);
    }
    double norm = sqrt(wave_norm(&w));
    for (int j = 0; j < w.n; j++) w.psi[j] /= norm;
    double p0 = creal(w.psi[400] * conj(w.psi[400])), a0 = creal(w.psi[400]), worst = 0.0;
    int n = 0, zero_step = -1;
    double prev = a0;
    for (int k = 0; k <= 2600; k++) {
        double re = creal(w.psi[400]), p = creal(w.psi[400] * conj(w.psi[400]));
        worst = fmax(worst, fabs(p / p0 - 1.0));
        if (zero_step < 0 && prev > 0.0 && re <= 0.0) zero_step = k;
        prev = re;
        if (k % 10 == 0) { xs[n] = (float)(k * 0.01); ya[n] = (float)(re / a0); yb[n] = (float)cos(0.5 * k * 0.01); yc[n] = (float)(p / p0); n++; }
        if (k < 2600) wave_step(&w);
    }
    value("ground-energy", "mean energy of the harmonic ground state on the grid (theory 1/2)", wave_energy(&w), 0.5);
    value("ground-density", "largest relative change of the density at the centre over t = 26, two periods of the phase (theory 0)", worst, 0.0);
    value("ground-phase", "time at which the real part first crosses zero (theory pi / (2 E) = pi)", zero_step * 0.01, PI);
    Series s[] = {{"Re psi(0) / start", ya, "#1f77b4"}, {"cos(t / 2)", yb, "#d62728"}, {"density / start", yc, "#2ca02c"}};
    plot("stationary.svg", "Harmonic ground state: the phase turns, the density stays", "t", "relative value at x = 0", n, s, 3);
    wave_free(&w);
}

/* S11: the ends of the grid are hard walls. */
static void fig_walls(void)
{
    Wave w;
    must(wave_init(&w, 1000, -50.0, 0.1, 0.05));
    wave_gaussian(&w, 20.0, 4.0, 1.0);
    int n = 0;
    double min_norm = 2.0, turn = -1e9, edge = 0.0;
    for (int k = 0; k <= 1200; k++) {
        double m = wave_mean_x(&w);
        if (k % 6 == 0) { xs[n] = (float)(k * 0.05); ya[n] = (float)m; yb[n] = (float)(50.0 * wave_norm(&w)); n++; }
        min_norm = fmin(min_norm, wave_norm(&w));
        turn = fmax(turn, m);
        edge = fmax(edge, cabs(w.psi[w.n - 1]));
        if (k < 1200) wave_step(&w);
    }
    value("wall-norm", "smallest total probability while a packet reflects off the end of the grid (theory 1: nothing leaves)", min_norm, 1.0);
    value("wall-return", "mean position after t = 60: started at 20 moving right at 1, wall at 49.9 (theory of a perfect mirror: 2 x 50 - 20 - 60 = 20)", wave_mean_x(&w), 20.0);
    value("wall-turn", "furthest the mean position gets (the wall is at 49.9)", turn, 0.0);
    Series s[] = {{"mean position", ya, "#1f77b4"}, {"50 x norm", yb, "#d62728"}};
    plot("walls.svg", "A packet reflecting off the hard wall at the end of the grid", "t", "x", n, s, 2);
    wave_free(&w);
    (void)edge;
}

/* S12..S14: the browser adapter. */
static void fig_adapter(void)
{
    qw_reset(1);
    int n = 0;
    double worst_sum = 0.0, edge = 0.0;
    while (!qw_value(QW_DONE) && n < MAXN) {
        xs[n] = (float)qw_value(QW_TIME);
        ya[n] = (float)qw_value(QW_P_LEFT);
        yb[n] = (float)qw_value(QW_P_RIGHT);
        yc[n] = (float)(10.0 * qw_value(QW_P_INSIDE));
        worst_sum = fmax(worst_sum, fabs(qw_value(QW_P_LEFT) + qw_value(QW_P_INSIDE) + qw_value(QW_P_RIGHT) - 1.0));
        n++;
        qw_step(15);
    }
    for (int b = 0; b < QW_BIN_COUNT; b++)
        if (b < 20 || b >= QW_BIN_COUNT - 20) edge += qw_density(b) * 1.0;
    double area = 0.0;
    for (int b = 0; b < QW_BIN_COUNT; b++) area += qw_density(b) * 1.0;   /* each bin is 10 sites x 0.1 */
    value("demo-steps", "time at the end of the standard demonstration (theory 3300 x 0.05)", qw_value(QW_TIME), 165.0);
    value("demo-right", "probability beyond the barrier at the end (theory: the packet formula shown on screen)", qw_value(QW_P_RIGHT), qw_value(QW_THEORY));
    value("demo-sum", "largest departure of left + inside + right from 1 during the run (theory 0)", worst_sum, 0.0);
    value("demo-bins-area", "sum of the 390 displayed densities x bin width 1 (theory 1)", area, 1.0);
    value("demo-edges", "probability in the 20 bins at each end of the grid at the end (the hard walls are not reached)", edge, 0.0);
    value("demo-energy", "QW_ENERGY at the end (theory: the packet's k0^2 / 2 + 1 / (8 sigma^2))", qw_value(QW_ENERGY), 0.50125);
    value("demo-locked", "barrier height after qw_controls is called in the demonstration (theory 0.6: controls locked)", (qw_controls(10.0, 1.2), qw_value(QW_HEIGHT)), 0.6);
    Series s[] = {{"left of the barrier", ya, "#1f77b4"}, {"beyond it", yb, "#d62728"}, {"inside x 10", yc, "#2ca02c"}};
    plot("demo.svg", "The standard demonstration: where the probability is", "t", "probability", n, s, 3);

    /* The half-through demonstration and the bisection behind it. */
    double half = qw_half_height();
    n = 0;
    for (double h = 0.0; h <= 1.5001; h += 0.01, n++) {
        xs[n] = (float)h;
        ya[n] = (float)packet_transmission(1.0, 10.0, h, 2.0);
        yb[n] = 0.5f;
    }
    value("half-height", "barrier height found by qw_half_height (the packet formula there should be 0.5)", packet_transmission(1.0, 10.0, half, 2.0), 0.5);
    value("half-height-value", "that height, in units where the packet's energy is 0.5", half, 0.0);
    qw_reset(2);
    while (!qw_value(QW_DONE)) qw_step(400);
    value("half-right", "probability beyond the barrier at the end of the half-through demonstration (theory 0.5)", qw_value(QW_P_RIGHT), 0.5);
    Series hs[] = {{"packet formula", ya, "#1f77b4"}, {"T = 0.5", yb, "#d62728"}};
    plot("half.svg", "Packet formula against height, and the height for T = 0.5", "barrier height V", "T", n, hs, 2);

    /* Manual mode: the controls are clamped and snapped. */
    qw_reset(0);
    qw_controls(13.2, 9.0);
    value("manual-left", "barrier position after asking for 13.2 (theory 14: snapped to the 2-wide grid of the barrier)", qw_value(QW_LEFT), 14.0);
    value("manual-height", "barrier height after asking for 9 (theory 1.5: clamped)", qw_value(QW_HEIGHT), 1.5);
    qw_controls(-500.0, -1.0);
    value("manual-left-min", "position after asking for -500 (theory -40)", qw_value(QW_LEFT), -40.0);
    value("manual-height-min", "height after asking for -1 (theory 0)", qw_value(QW_HEIGHT), 0.0);
    qw_step(5000);
    value("manual-clamp", "time after one call asking for 5000 steps (theory 400 x 0.05: one call is clamped)", qw_value(QW_TIME), 20.0);
    qw_launch();
    value("manual-launch", "time after qw_launch (theory 0: a fresh packet)", qw_value(QW_TIME), 0.0);
    value("manual-real", "qw_real in the bin just right of the packet centre over the square root of qw_density there: the bin centre is 0.5 from the packet centre (theory cos(k0 x 0.5))", qw_real(95) / sqrt(qw_density(95)), cos(0.5));
}

int main(int argc, char **argv)
{
    snprintf(dir, sizeof dir, "%s", argc > 1 ? argv[1] : "docs/theory/figures");
    values = fopen(path("values.md"), "w");
    if (!values) { fprintf(stderr, "cannot write %s/values.md\n", dir); return 1; }
    fprintf(values, "# Measured values\n\nWritten by `make theory-figs` (tests/theory_figs.c) from the program's own functions. Do not edit by hand.\n"
                    "Units: hbar = m = 1. The last column is the relative error; where the theory column is 0 it is the absolute value.\n\n"
                    "| id | what is measured | measured | theory | error |\n|---|---|---|---|---|\n");
    fig_conservation();
    fig_packet();
    fig_dispersion();
    fig_current();
    fig_barrier();
    fig_experiment();
    fig_unitarity();
    val_thomas();
    fig_order();
    fig_stationary();
    fig_walls();
    fig_adapter();
    fclose(values);
    return 0;
}
