#ifndef OBSERVER_H
#define OBSERVER_H

/* Browser adapter: one Wave on the same grid as the terminal game, read through plain doubles. */
enum {
    QW_TIME, QW_LEFT, QW_HEIGHT, QW_WIDTH, QW_NORM, QW_MEAN_X, QW_P_LEFT, QW_P_INSIDE,
    QW_P_RIGHT, QW_ENERGY, QW_THEORY, QW_MODE, QW_K0, QW_SIGMA, QW_X_MIN, QW_X_MAX,
    QW_BINS, QW_DONE, QW_FIELDS
};
#define QW_BIN_COUNT 390
#define QW_STEPS_PER_RUN 3300

/* mode: 0 manual barrier, 1 standard barrier demo (V 0.6), 2 half-through demo (theory T = 0.5). */
int qw_reset(int mode);
void qw_controls(double left, double height);
void qw_launch(void);
void qw_step(int steps);
double qw_value(int field);
double qw_density(int bin);   /* mean |psi|^2 over the bin */
double qw_real(int bin);      /* Re psi at the bin centre */
double qw_half_height(void);  /* barrier height whose packet-averaged theory gives T = 0.5 */

#endif
