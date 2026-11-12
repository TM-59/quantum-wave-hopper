/* The browser's two-barrier experiment (src/resonance.c) against direct calls of the solver.
 * `./test_resonance --trace` prints the native samples the browser smoke compares with WASM. */
#include "../src/resonance.h"
#include "../src/wave.h"

#include <complex.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

static int failures;
#define CHECK(cond, ...)                                                    \
    do {                                                                    \
        if (!(cond)) { failures++; printf("FAIL %s:%d: ", __FILE__, __LINE__); printf(__VA_ARGS__); printf("\n"); } \
    } while (0)

static void finish_run(void)
{
    for (int guard = 0; guard < 1000 && !rs_value(RS_DONE); guard++) rs_step(400);
}

static void test_resonances_found(void)
{
    CHECK(rs_reset(0) == 0, "allocation failed");
    double e = 0.0, g = 0.0;
    CHECK(double_barrier_resonance(1.0, 1.0, 4.0, 0.01, 1.0, &e, &g) == 0, "first resonance");
    CHECK(rs_value(RS_E1) == e && rs_value(RS_G1) == g, "adapter E1 %.9f G1 %.9f", rs_value(RS_E1), rs_value(RS_G1));
    CHECK(rs_value(RS_E1) < rs_value(RS_E2) && rs_value(RS_E2) < rs_value(RS_HEIGHT), "two resonances below the top: %.4f %.4f",
          rs_value(RS_E1), rs_value(RS_E2));
    CHECK(rs_value(RS_G2) > 5.0 * rs_value(RS_G1), "the higher resonance is broader: %.4f vs %.4f", rs_value(RS_G2), rs_value(RS_G1));
}

static void test_controls_are_clamped(void)
{
    rs_reset(0);
    rs_energy(5.0);
    CHECK(rs_value(RS_ENERGY) == RS_E_MAX, "energy clamped to %.2f, got %.4f", RS_E_MAX, rs_value(RS_ENERGY));
    rs_energy(-1.0);
    CHECK(rs_value(RS_ENERGY) == RS_E_MIN, "energy clamped to %.2f", RS_E_MIN);
    rs_energy(0.3);
    rs_step(5000);
    CHECK(fabs(rs_value(RS_TIME) - 20.0) < 1e-12, "one call is clamped to 400 steps: t %.3f", rs_value(RS_TIME));
    rs_launch();
    CHECK(rs_value(RS_TIME) == 0.0, "launch restarts");
    CHECK(fabs(rs_value(RS_NORM) - 1.0) < 1e-12, "fresh packet normalised");
}

static void test_packet_on_resonance(void)
{
    rs_reset(0);
    rs_energy(rs_value(RS_E1));
    double peak_well = 0.0;
    for (int guard = 0; guard < 1000 && !rs_value(RS_DONE); guard++) {
        rs_step(100);
        peak_well = fmax(peak_well, rs_value(RS_P_WELL));
        CHECK(fabs(rs_value(RS_P_LEFT) + rs_value(RS_P_WELL) + rs_value(RS_P_RIGHT) + 2.0 * 0.0 - 1.0) < 0.2, "regions");
    }
    double sum = 0.0;
    for (int b = 0; b < RS_BIN_COUNT; b++) sum += rs_density(b);
    CHECK(rs_value(RS_DONE) == 1.0, "run finished");
    CHECK(fabs(rs_value(RS_NORM) - 1.0) < 1e-10, "norm %.12f", rs_value(RS_NORM));
    CHECK(fabs(rs_value(RS_P_RIGHT) / rs_value(RS_T_PACKET) - 1.0) < 0.01, "on resonance: simulated %.5f vs packet theory %.5f",
          rs_value(RS_P_RIGHT), rs_value(RS_T_PACKET));
    CHECK(rs_value(RS_P_RIGHT) > 0.45, "on resonance about half the packet passes: %.4f", rs_value(RS_P_RIGHT));
    CHECK(peak_well > 0.05, "probability piles up between the barriers: %.4f", peak_well);
    CHECK(rs_value(RS_P_WELL) < 1e-3, "the well empties by the end: %.2e", rs_value(RS_P_WELL));
    CHECK(sum > 0.0, "density visible");
}

static void test_packet_off_resonance(void)
{
    rs_reset(0);
    rs_energy(0.40);
    finish_run();
    CHECK(fabs(rs_value(RS_P_RIGHT) / rs_value(RS_T_PACKET) - 1.0) < 0.02, "off resonance: simulated %.5f vs packet theory %.5f",
          rs_value(RS_P_RIGHT), rs_value(RS_T_PACKET));
    CHECK(rs_value(RS_P_RIGHT) < 0.1, "off resonance little passes: %.4f", rs_value(RS_P_RIGHT));
}

static void test_well_release(void)
{
    rs_reset(1);
    CHECK(fabs(rs_value(RS_P_WELL) - 1.0) < 1e-12, "starts in the well: %.12f", rs_value(RS_P_WELL));
    /* Least-squares slope of ln P_well between t = 30 and t = 200. */
    double sx = 0.0, sy = 0.0, sxx = 0.0, sxy = 0.0;
    int m = 0;
    while (!rs_value(RS_DONE)) {
        double t = rs_value(RS_TIME);
        if (t >= 30.0 - 1e-9) {
            double y = log(rs_value(RS_P_WELL));
            sx += t; sy += y; sxx += t * t; sxy += t * y; m++;
        }
        rs_step(20);
    }
    double rate = -(m * sxy - sx * sy) / (m * sxx - sx * sx);
    CHECK(fabs(rate / rs_value(RS_G1) - 1.0) < 0.03, "decay rate %.5f vs width %.5f", rate, rs_value(RS_G1));
    CHECK(fabs(rs_value(RS_TIME) - 200.0) < 1e-9, "the release runs to t = 200");
}

static void test_curves(void)
{
    int best = 0;
    for (int i = 0; i < RS_CURVE_COUNT; i++) if (rs_curve(i) > rs_curve(best)) best = i;
    double e0 = rs_value(RS_CURVE_E0), e1 = rs_value(RS_CURVE_E1), eb = e0 + (e1 - e0) * best / (RS_CURVE_COUNT - 1);
    CHECK(fabs(eb - rs_value(RS_E1)) < 0.01 || fabs(eb - rs_value(RS_E2)) < 0.01, "curve peaks at a resonance: %.4f", eb);
    CHECK(rs_curve(-1) == 0.0 && rs_curve(RS_CURVE_COUNT) == 0.0, "out of range");
    for (int i = 0; i < RS_CURVE_COUNT; i++) CHECK(rs_curve_packet(i) <= 1.0 && rs_curve_packet(i) >= 0.0, "packet curve in [0, 1]");
}

/* Native samples for the WASM comparison of scripts/smoke.cjs. */
static void trace(void)
{
    printf("mode,step");
    for (int f = 0; f < RS_FIELDS; f++) printf(",f%d", f);
    printf(",d150,d200,d205,z30,z60,c20,c40,p20\n");
    for (int m = 0; m < 2; m++) {
        rs_reset(m);
        if (m == 0) rs_energy(0.30);
        for (int step = 0; step <= 2000; step += 100) {
            printf("%d,%d", m, step);
            for (int f = 0; f < RS_FIELDS; f++) printf(",%.17g", rs_value(f));
            printf(",%.17g,%.17g,%.17g,%.17g,%.17g,%.17g,%.17g,%.17g\n", rs_density(150), rs_density(200), rs_density(205), rs_zoom(30),
                   rs_zoom(60), rs_curve(20), rs_curve(40), rs_curve_packet(20));
            rs_step(100);
        }
    }
}

int main(int argc, char **argv)
{
    if (argc > 1 && strcmp(argv[1], "--trace") == 0) { trace(); return 0; }
    test_resonances_found();
    test_controls_are_clamped();
    test_packet_on_resonance();
    test_packet_off_resonance();
    test_well_release();
    test_curves();
    if (failures) { printf("%d check(s) failed\n", failures); return 1; }
    puts("resonance adapter: all tests passed");
    return 0;
}
