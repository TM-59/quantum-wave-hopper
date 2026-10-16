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
    if (argc > 1 && strcmp(argv[1], "--gif") == 0) return gif_demo(argc > 2 ? argv[2] : "demo.gif");
    return play();
}
