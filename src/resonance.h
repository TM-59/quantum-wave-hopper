#ifndef RESONANCE_H
#define RESONANCE_H

/* Browser adapter of the two-barrier experiment (M2): one Wave on its own grid, read through plain
 * doubles. Two barriers of height 1 and width 1 at [0, 1) and [5, 6), a well of width 4 between them. */
enum {
    RS_TIME, RS_MODE, RS_ENERGY, RS_K0, RS_SIGMA, RS_HEIGHT, RS_WIDTH, RS_GAP, RS_NORM,
    RS_P_LEFT, RS_P_WELL, RS_P_RIGHT, RS_T_PLANE, RS_T_PACKET, RS_E1, RS_G1, RS_E2, RS_G2,
    RS_DONE, RS_T_MAX, RS_VIEW_MIN, RS_VIEW_MAX, RS_BINS, RS_ZOOM_MIN, RS_ZOOM_MAX, RS_ZOOM_SITES,
    RS_CURVE_E0, RS_CURVE_E1, RS_CURVE_POINTS, RS_FIELDS
};
#define RS_BIN_COUNT 400        /* the view x in [-200, 200), bins of 10 sites (width 1) */
#define RS_ZOOM_COUNT 120       /* every site of x in [-3, 9) */
#define RS_CURVE_COUNT 240      /* T(E) for E in [0.02, 0.80] */
#define RS_E_MIN 0.05
#define RS_E_MAX 0.75

/* mode 0: a packet (sigma 20) sent at the chosen energy; mode 1: the lowest box state released from the well. */
int rs_reset(int mode);
void rs_energy(double energy);   /* clamped to [RS_E_MIN, RS_E_MAX]; launches a fresh packet in mode 0 */
void rs_launch(void);
void rs_step(int steps);         /* at most 400 per call; stops at the end of the run */
double rs_value(int field);
double rs_density(int bin);      /* mean |psi|^2 over the bin of the view */
double rs_zoom(int site);        /* |psi|^2 at a site of the zoom around the barriers */
double rs_curve(int point);      /* plane-wave T at the point's energy */
double rs_curve_packet(int point); /* the same averaged over the packet's momenta */

#endif
