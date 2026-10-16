#ifndef RENDER_H
#define RENDER_H

#include <stddef.h>

#include "wave.h"

#define RENDER_COLS 78
#define RENDER_ROWS 20

/* Probability density as vertical bars (the grid is binned into RENDER_COLS columns, the tallest
 * bin reaches the top at density `scale`), '=' marks the potential on the bottom line.
 * Returns bytes written or 0 if cap is too small. */
size_t render_frame(const Wave *w, double scale, char *buf, size_t cap);

#endif
