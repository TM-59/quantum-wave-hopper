#include "render.h"

#include <math.h>

size_t render_frame(const Wave *w, double scale, char *buf, size_t cap)
{
    if (cap < (size_t)(RENDER_COLS + 1) * RENDER_ROWS + 1) return 0;
    char g[RENDER_ROWS][RENDER_COLS];
    for (int i = 0; i < RENDER_ROWS; i++)
        for (int j = 0; j < RENDER_COLS; j++) g[i][j] = ' ';
    int per = (w->n + RENDER_COLS - 1) / RENDER_COLS;
    for (int c = 0; c < RENDER_COLS; c++) {
        double peak = 0.0, pot = 0.0;
        for (int j = c * per; j < (c + 1) * per && j < w->n; j++) {
            double p = creal(w->psi[j] * conj(w->psi[j]));
            if (p > peak) peak = p;
            if (w->v[j] > pot) pot = w->v[j];
        }
        int h = (int)lround(peak / scale * (RENDER_ROWS - 1));
        if (h > RENDER_ROWS - 1) h = RENDER_ROWS - 1;
        for (int i = 0; i < h; i++) g[RENDER_ROWS - 2 - i][c] = '#';
        if (pot > 0.0) g[RENDER_ROWS - 1][c] = '=';
    }
    size_t used = 0;
    for (int i = 0; i < RENDER_ROWS; i++) {
        for (int j = 0; j < RENDER_COLS; j++) buf[used++] = g[i][j];
        buf[used++] = '\n';
    }
    buf[used] = '\0';
    return used;
}
