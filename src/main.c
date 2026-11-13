#define _POSIX_C_SOURCE 200809L
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <termios.h>
#include <time.h>
#include <unistd.h>

#include "gif.h"
#include "render.h"
#include "slits.h"
#include "svg.h"
#include "wave.h"

#define GRID 3900
#define DX 0.1
#define SIGMA 10.0
#define K0 1.0
#define START -100.0

/* Density snapshots of a packet hitting a barrier, plus T(k0) from simulation vs theory. */
static int headless(const char *dir)
{
    char path[512];
    Wave w;
    if (wave_init(&w, 4000, -200.0, DX, 0.05)) return 1;
    wave_gaussian(&w, START, SIGMA, K0);
    wave_set_barrier(&w, 0.0, 2.0, 0.6);
    enum { M = 800 };
    static float x[M], d0[M], d1[M], d2[M];
    int stride = w.n / M;
    for (int snap = 0; snap < 3; snap++) {
        float *d = snap == 0 ? d0 : snap == 1 ? d1 : d2;
        for (int j = 0; j < M; j++) {
            double p = creal(w.psi[j * stride] * conj(w.psi[j * stride]));
            d[j] = (float)p;
            x[j] = (float)wave_x(&w, j * stride);
        }
        for (int k = 0; k < 1800 && snap < 2; k++) wave_step(&w);
    }
    snprintf(path, sizeof path, "%s/density.svg", dir);
    Series sd[] = {{"t = 0", d0, "#1f77b4"}, {"t = 90", d1, "#ff7f0e"}, {"t = 180", d2, "#2ca02c"}};
    if (svg_plot(path, "Wave packet hitting a barrier (V = 0.6, width 2)", "x", "|psi|^2", x, M, sd, 3)) return 1;
    wave_free(&w);

    enum { K = 11 };
    static float ks[K], tn[K], tt[K];
    snprintf(path, sizeof path, "%s/run.csv", dir);
    FILE *csv = fopen(path, "w");
    if (!csv) return 1;
    fprintf(csv, "k0,T_numeric,T_theory\n");
    for (int i = 0; i < K; i++) {
        double k0 = 0.6 + 0.1 * i;
        ks[i] = (float)k0;
        tn[i] = (float)simulate_transmission(k0, SIGMA, 0.6, 2.0);
        tt[i] = (float)packet_transmission(k0, SIGMA, 0.6, 2.0);
        fprintf(csv, "%.2f,%.5f,%.5f\n", k0, tn[i], tt[i]);
    }
    fclose(csv);
    snprintf(path, sizeof path, "%s/transmission.svg", dir);
    Series st[] = {{"simulation", tn, "#d62728"}, {"theory (packet-averaged)", tt, "#1f77b4"}};
    if (svg_plot(path, "Barrier transmission vs wavenumber", "k0", "T", ks, K, st, 2)) return 1;
    printf("T(k0=1.0): numeric %.4f, theory %.4f\n", tn[4], tt[4]);
    return 0;
}

/* Two barriers (height 1, width 1, gap 4): T(E) by transfer matrices and averaged over packets,
 * plus simulated packets at nine energies (resonance.csv). */
static int resonance(const char *dir)
{
    char path[512];
    enum { N = 391 };
    static float e[N], tp[N], t20[N], t10[N];
    for (int i = 0; i < N; i++) {
        double en = 0.02 + 0.002 * i, k = sqrt(2.0 * en);
        e[i] = (float)en;
        tp[i] = (float)double_barrier_transmission(k, 1.0, 1.0, 4.0);
        t20[i] = (float)double_packet_transmission(k, 20.0, 1.0, 1.0, 4.0);
        t10[i] = (float)double_packet_transmission(k, 10.0, 1.0, 1.0, 4.0);
    }
    snprintf(path, sizeof path, "%s/resonance.svg", dir);
    Series s[] = {{"plane wave", tp, "#1f77b4"}, {"packet sigma 20", t20, "#d62728"}, {"packet sigma 10", t10, "#2ca02c"}};
    if (svg_plot(path, "Two barriers (V = 1, width 1, gap 4): resonant tunnelling", "energy E", "T", e, N, s, 3)) return 1;
    double e1, g1, e2, g2;
    if (double_barrier_resonance(1.0, 1.0, 4.0, 0.01, 1.0, &e1, &g1) ||
        double_barrier_resonance(1.0, 1.0, 4.0, e1 + 3.0 * g1, 1.0, &e2, &g2)) return 1;
    snprintf(path, sizeof path, "%s/resonance.csv", dir);
    FILE *csv = fopen(path, "w");
    if (!csv) return 1;
    fprintf(csv, "E,T_plane,T_packet_sigma10,T_simulated_sigma10\n");
    double es[9] = {0.08, 0.12, e1, 0.20, 0.30, 0.45, e2, 0.70, 0.80};
    for (int i = 0; i < 9; i++) {
        double k = sqrt(2.0 * es[i]);
        fprintf(csv, "%.6f,%.5f,%.5f,%.5f\n", es[i], double_barrier_transmission(k, 1.0, 1.0, 4.0),
                double_packet_transmission(k, 10.0, 1.0, 1.0, 4.0), simulate_double_transmission(k, 10.0, 1.0, 1.0, 4.0));
    }
    fclose(csv);
    printf("resonances: E1 %.5f (width %.5f, lifetime %.1f), E2 %.5f (width %.5f, lifetime %.2f)\n", e1, g1, 1.0 / g1, e2, g2, 1.0 / g2);
    return 0;
}

/* The double slit (M3): the browser's 2D experiment run to the end in three modes; the screen at L = 100
 * against Fraunhofer (slits.svg, slits.csv) and an animation of the wave with particles landing (slits.gif). */
static int slit_density_index(const unsigned char *px)
{
    if (px[0] == 240 && px[1] == 228 && px[2] == 200) return 1;
    int top = px[0] > px[1] ? px[0] : px[1];
    if (px[2] > top) top = px[2];
    return top < 8 ? 0 : 2 + (top * 14 - 1) / 256;
}

static int slits_gif(const char *path)
{
    enum { W = 256, PANEL = 160, GW = W + PANEL, H = 256 };
    unsigned char pal[GIF_COLOURS][3] = {{8, 10, 24}, {240, 228, 200}};
    for (int c = 2; c < GIF_COLOURS; c++) {
        double f = (c - 1) / 14.0;
        pal[c][0] = (unsigned char)lround(255.0 * fmin(1.0, 1.6 * f * f));
        pal[c][1] = (unsigned char)lround(255.0 * fmin(1.0, 1.2 * f));
        pal[c][2] = (unsigned char)lround(255.0 * fmin(1.0, 0.25 + f));
    }
    static unsigned char px[GW * H];
    static double counts[H];
    Gif *g = gif_open(path, GW, H, pal, 5);
    if (!g) return 1;
    sl_reset(0);
    sl_seed(1);
    double top = 0.0;
    for (int m = 0; m < SL_BINS; m++) top = fmax(top, 1.15 * sl_screen_theory(m));
    for (int frame = 0;; frame++) {
        int done = (int)sl_value(SL_DONE), particles = done ? (frame - (int)(sl_value(SL_STEPS) / 15) - 1) * 200 : 0;
        if (particles < 0) particles = 0;
        if (frame % 3 == 0 || done) sl_measure();
        const unsigned char *img = sl_image();
        memset(px, 0, sizeof px);
        for (int r = 0; r < H; r++)
            for (int c = 0; c < W; c++) px[r * GW + c] = (unsigned char)slit_density_index(img + 4 * (r * W + c));
        for (int r = 0; r < H; r++) px[r * GW + W] = 1;
        /* Right panel: the screen at L = 100, y from +100 (top) to -100. Measured intensity in bright bars,
         * the Fraunhofer curve as a beige dot per row; after the run, particles counted per row. */
        double scale = top;
        memset(counts, 0, sizeof counts);
        for (int n = 0; n < particles; n++) {
            int r = (int)floor((sl_value(SL_SCREEN_HALF) - sl_particle(n)) / (2.0 * sl_value(SL_SCREEN_HALF)) * H);
            if (r >= 0 && r < H) counts[r]++;
        }
        double ctop = 1.0;
        for (int r = 0; r < H; r++) ctop = fmax(ctop, counts[r]);
        for (int r = 0; r < H; r++) {
            int m = (int)((double)(H - 1 - r) / H * SL_BINS);
            int bar = particles ? (int)lround(counts[r] / fmax(ctop, 0.02 * particles) * (PANEL - 12))
                                : (int)lround(sl_screen(m) / scale * (PANEL - 12));
            for (int c = 0; c < bar && c < PANEL - 4; c++) px[r * GW + W + 4 + c] = particles ? 11 : 15;
            int t = (int)lround(sl_screen_theory(m) / scale * (PANEL - 12));
            if (done && t < PANEL - 4) px[r * GW + W + 4 + t] = 1;
        }
        if (gif_frame(g, px, done && particles >= 6000 ? 300 : 0)) return 1;
        if (done && particles >= 6000) break;
        if (!done) sl_step(15);
    }
    return gif_close(g);
}

static int slits(const char *dir)
{
    char path[512];
    static float y[SL_BINS], both[SL_BINS], both_th[SL_BINS], one[SL_BINS], one_th[SL_BINS], watched[SL_BINS], watched_th[SL_BINS];
    static double ideal[SL_BINS];
    float *meas[3] = {both, one, watched}, *th[3] = {both_th, one_th, watched_th};
    int modes[3] = {0, 1, 3};
    double vis[3], spacing = 0.0, exact = 0.0;
    for (int i = 0; i < 3; i++) {
        if (sl_reset(modes[i])) return 1;
        while (!sl_value(SL_DONE)) sl_step(50);
        for (int m = 0; m < SL_BINS; m++) {
            y[m] = (float)(-sl_value(SL_SCREEN_HALF) + (m + 0.5) * 2.0 * sl_value(SL_SCREEN_HALF) / SL_BINS);
            meas[i][m] = (float)sl_screen(m);
            th[i][m] = (float)sl_screen_theory(m);
            if (i == 0) ideal[m] = sl_screen_ideal(m);
        }
        vis[i] = sl_value(SL_VISIBILITY);
        if (i == 0) { spacing = sl_value(SL_SPACING_MEASURED); exact = sl_value(SL_SPACING); }
    }
    snprintf(path, sizeof path, "%s/slits.svg", dir);
    Series s[] = {{"two slits (2D solver)", both, "#1f77b4"}, {"two slits (Fraunhofer)", both_th, "#888888"},
                  {"one slit (2D solver)", one, "#d62728"}, {"watched (2D solver)", watched, "#2ca02c"}};
    if (svg_plot(path, "Double slit (lambda 2.5, d 8.8, a 2.2): intensity on a screen 100 away", "screen position y", "intensity (area 1)", y, SL_BINS, s, 4)) return 1;
    snprintf(path, sizeof path, "%s/slits.csv", dir);
    FILE *csv = fopen(path, "w");
    if (!csv) return 1;
    fprintf(csv, "y,two_slits,two_slits_fraunhofer,two_slits_ideal,one_slit,one_slit_fraunhofer,watched,watched_fraunhofer\n");
    for (int m = 0; m < SL_BINS; m++)
        fprintf(csv, "%.1f,%.6f,%.6f,%.6f,%.6f,%.6f,%.6f,%.6f\n", y[m], both[m], both_th[m], ideal[m], one[m], one_th[m], watched[m], watched_th[m]);
    fclose(csv);
    snprintf(path, sizeof path, "%s/slits.gif", dir);
    if (slits_gif(path)) return 1;
    printf("double slit: first fringe %.2f (L tan asin(lambda/d) %.2f); visibility two slits %.3f, one slit %.3f, watched %.3f\n",
           spacing, exact, vis[0], vis[1], vis[2]);
    return 0;
}

static int snapshot(void)
{
    Wave w;
    if (wave_init(&w, GRID, -195.0, DX, 0.05)) return 1;
    wave_gaussian(&w, START, SIGMA, K0);
    wave_set_barrier(&w, 0.0, 2.0, 0.6);
    for (int k = 0; k < 1800; k++) wave_step(&w);
    static char buf[RENDER_COLS * RENDER_ROWS * 4];
    int ok = render_frame(&w, 0.05, buf, sizeof buf) != 0;
    if (ok) fputs(buf, stdout);
    wave_free(&w);
    return ok ? 0 : 1;
}

static int gif_colour(char c) { return c == '=' ? 2 : 3; }

/* Scripted run: a wave packet hits the barrier and splits into reflected and transmitted parts. */
static int gif_demo(const char *path)
{
    Wave w;
    if (wave_init(&w, GRID, -195.0, DX, 0.05)) return 1;
    wave_gaussian(&w, START, SIGMA, K0);
    wave_set_barrier(&w, 0.0, 2.0, 0.6);
    unsigned char pal[GIF_COLOURS][3] = {{8, 10, 24}, {235, 235, 235}, {255, 205, 60}, {90, 210, 255}};
    const int scale = 3, cw = RENDER_COLS * 6 * scale, ch = RENDER_ROWS * 9 * scale;
    static unsigned char px[RENDER_COLS * 6 * 3 * RENDER_ROWS * 9 * 3];
    static char buf[RENDER_COLS * RENDER_ROWS * 4];
    Gif *g = gif_open(path, cw, ch, pal, 6);
    if (!g) return 1;
    enum { STEPS = 3300, EVERY = 33 };
    for (int k = 0; k <= STEPS; k++) {
        if (k % EVERY == 0) {
            render_frame(&w, 0.05, buf, sizeof buf);
            gif_draw_text(px, cw, ch, buf, scale, gif_colour);
            if (gif_frame(g, px, k == STEPS ? 250 : 0)) return 1;
        }
        wave_step(&w);
    }
    double right = wave_prob_right(&w, 0.0);
    wave_free(&w);
    if (gif_close(g)) return 1;
    printf("probability transmitted: %.3f\n", right);
    return 0;
}

static struct termios saved;
static void restore(void) { tcsetattr(STDIN_FILENO, TCSANOW, &saved); fputs("\033[?25h", stdout); }

static void launch(Wave *w, double left, double height)
{
    wave_gaussian(w, START, SIGMA, K0);
    wave_set_barrier(w, left, 2.0, height);
}

static int play(void)
{
    if (!isatty(STDIN_FILENO)) { fputs("interactive mode needs a terminal; try --snapshot\n", stderr); return 1; }
    Wave w;
    if (wave_init(&w, GRID, -195.0, DX, 0.05)) return 1;
    double left = 0.0, height = 0.6;
    launch(&w, left, height);
    tcgetattr(STDIN_FILENO, &saved);
    struct termios raw = saved;
    raw.c_lflag &= ~(ICANON | ECHO);
    raw.c_cc[VMIN] = 0;
    raw.c_cc[VTIME] = 0;
    tcsetattr(STDIN_FILENO, TCSANOW, &raw);
    atexit(restore);
    fputs("\033[2J\033[?25l", stdout);

    static char buf[RENDER_COLS * RENDER_ROWS * 4];
    struct timespec tick = {0, 40 * 1000000L};
    int paused = 0;
    double t = 0.0;
    for (;;) {
        char c;
        int changed = 0;
        while (read(STDIN_FILENO, &c, 1) == 1) {
            if (c == 'q') return 0;
            if (c == 'p') paused = !paused;
            if (c == 'a') { left -= 2.0; changed = 1; }
            if (c == 'd') { left += 2.0; changed = 1; }
            if (c == 'w') { height += 0.1; changed = 1; }
            if (c == 's') { height = fmax(0.0, height - 0.1); changed = 1; }
            if (c == ' ') { launch(&w, left, height); t = 0.0; }
        }
        if (changed) wave_set_barrier(&w, left, 2.0, height);
        if (!paused) {
            for (int k = 0; k < 8; k++) wave_step(&w);
            t += 8 * w.dt;
        }
        render_frame(&w, 0.05, buf, sizeof buf);
        printf("\033[H%s", buf);
        printf("t %6.1f  barrier V=%.1f at x=%.0f  right of barrier: %5.1f%%  norm %.4f   \n", t, height,
               left, 100.0 * wave_prob_right(&w, left + 2.0), wave_norm(&w));
        puts("space relaunch  a/d move barrier  w/s height  p pause  q quit    goal: let exactly half through");
        fflush(stdout);
        nanosleep(&tick, NULL);
    }
}

int main(int argc, char **argv)
{
    if (argc > 1 && strcmp(argv[1], "--headless") == 0) return headless(argc > 2 ? argv[2] : ".");
    if (argc > 1 && strcmp(argv[1], "--snapshot") == 0) return snapshot();
    if (argc > 1 && strcmp(argv[1], "--resonance") == 0) return resonance(argc > 2 ? argv[2] : ".");
    if (argc > 1 && strcmp(argv[1], "--slits") == 0) return slits(argc > 2 ? argv[2] : ".");
    if (argc > 1 && strcmp(argv[1], "--gif") == 0) return gif_demo(argc > 2 ? argv[2] : "demo.gif");
    return play();
}
