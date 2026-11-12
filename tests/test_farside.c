/* Story layer checks: "The Far Side" changes game numbers only, never the solver's share. */
#include "farside_run.h"
#include "../src/observer.h"
#include "../src/wave.h"

#include <complex.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

static int checks, failures;
#define CHECK(cond, ...) do { checks++; if (!(cond)) { failures++; printf("FAIL %s:%d ", __FILE__, __LINE__); \
    printf(__VA_ARGS__); printf("\n"); } } while (0)

/* The terminal game's Wave API on the observer's grid: packet sigma 10, k0 1 from x = -100, barrier [0, 2). */
static double wave_api_share(double height)
{
    Wave w;
    if (wave_init(&w, 3900, -195.0, 0.1, 0.05)) return -1.0;
    wave_gaussian(&w, -100.0, 10.0, 1.0);
    wave_set_barrier(&w, 0.0, 2.0, height);
    for (int s = 0; s < QW_STEPS_PER_RUN; s++) wave_step(&w);
    double p = wave_prob_right(&w, 2.0);
    wave_free(&w);
    return p;
}

/* Play with the operator until `scene` opens (answering the others with the given choices). Returns 0 when it opened. */
static int play_until(Farside *c, Operator *p, int scene, int frost, int cells)
{
    for (int guard = 0; guard < RUN_GUARD_ACTIONS; guard++) {
        if (c->scene == scene) return 0;
        if (c->scene != FS_NONE) {
            farside_answer(c, c->scene == FS_FROST ? frost : c->scene == FS_FALL ? cells : 0);
            continue;
        }
        if (c->night >= F_NIGHTS) return 1;
        if (op_decide(p, c) == OP_SEAL) farside_seal(c); else farside_fire(c);
    }
    return 1;
}

static void start(Farside *c, Operator *p, unsigned effects, int scene, int frost, int cells, int seed)
{
    farside_init(c, effects, sub_seed(ablation_key(seed), 0, 9));
    op_init(p, ablation_key(seed));
    play_until(c, p, scene, frost, cells);
}

static double mean_metric(unsigned effects, int frost, int cells, int which)
{
    double s = 0.0;
    for (int i = 0; i < ABLATION_SEEDS; i++) {
        RunResult r;
        run_nights(ablation_key(i), effects, frost, cells, &r);
        s += which == 0 ? r.hours : r.failures;
    }
    return s / ABLATION_SEEDS;
}

int main(void)
{
    /* [physics] The share is the solver's: the Wave API run, bit for bit, for every dial step tried, whatever the story. */
    for (int step = 0; step <= 30; step += 5) {
        double api = wave_api_share(step * 0.05);
        CHECK(farside_transmission(step) == api, "[physics] step %d: story %.17g, Wave API %.17g", step, farside_transmission(step), api);
    }
    {
        /* The observatory is untouched by the story: its standard experiment reads the same after story nights. */
        qw_reset(1);
        qw_step(400); qw_step(400);
        double before = qw_value(QW_P_RIGHT);
        RunResult r;
        run_nights(play_key(0), FE_ALL, 0, 1, &r);
        run_nights(play_key(1), 0u, 1, 0, &r);
        qw_reset(1);
        qw_step(400); qw_step(400);
        CHECK(qw_value(QW_P_RIGHT) == before, "[physics] observatory after story runs %.17g vs %.17g", qw_value(QW_P_RIGHT), before);
        /* Story numbers on or off, the same seed and dial give the same far-side draws for the same particles. */
        Farside a, b;
        farside_init(&a, FE_ALL & ~FE_DUSK, 7);
        farside_init(&b, 0u, 7);
        farside_answer(&a, 0); farside_answer(&b, 0);
        farside_dial(&a, 10); farside_dial(&b, 10);
        int same = 1;
        for (int i = 0; i < 6; i++) { farside_fire(&a); farside_fire(&b); same &= a.last_k == b.last_k && a.last_n == b.last_n; }
        CHECK(same, "[physics] the counter's draws depend on the effects");
        /* Born's rule: the pooled count converges to the solver's share (4 sigma over 6,000 particles). */
        Farside c;
        farside_init(&c, 0u, 99);
        farside_answer(&c, 0);
        farside_dial(&c, 12);
        int n = 0, k = 0;
        for (int i = 0; i < 375; i++) { c.power = 8.0; farside_fire(&c); n += c.last_n; k += c.last_k; }
        double t = farside_transmission(12), sd = sqrt(t * (1.0 - t) / n);
        CHECK(fabs((double)k / n - t) < 4.0 * sd, "[physics] pooled %.4f vs share %.4f", (double)k / n, t);
    }

    /* [order] Scenes open at fixed progress points, in the same order on every path. */
    for (int i = 0; i < PLAY_SEEDS; i++) {
        RunResult r;
        int frost, cells;
        play_choices(i, &frost, &cells);
        run_nights(play_key(i), FE_ALL, frost, cells, &r);
        int ok = r.completed && r.order_len == FS_COUNT;
        for (int k = 0; ok && k < FS_COUNT; k++) ok = r.order[k] == k;
        CHECK(ok, "[order] seed %d", i);
        CHECK(r.ending == cells, "[order] seed %d ending %d", i, r.ending);
    }

    Farside c;
    Operator p;
    /* [mech] dusk: Agnes's copper counter sends more particles per shot, and she reads every seal again. */
    farside_init(&c, FE_ALL, 1); farside_answer(&c, 0);
    CHECK(farside_particles(&c) == 60 && farside_second_reading(&c), "[mech] dusk on: %d particles", farside_particles(&c));
    farside_init(&c, FE_ALL & ~FE_DUSK, 1); farside_answer(&c, 0);
    CHECK(farside_particles(&c) == 16 && !farside_second_reading(&c), "[mech] dusk off: %d particles", farside_particles(&c));
    {
        /* Her second reading: a seal she reads outside the band is refused ("not yet"), with no failure. */
        farside_init(&c, FE_ALL, 3); farside_answer(&c, 0);
        farside_dial(&c, 0);   /* V = 0: everything goes through, far outside the band of night 1 */
        farside_fire(&c);
        int r = farside_seal(&c);
        CHECK(r == 2 && c.failures == 0 && c.banner == FB_NOT_YET, "[mech] dusk second reading: %d, failures %d", r, c.failures);
        farside_init(&c, FE_ALL & ~FE_DUSK, 3); farside_answer(&c, 0);
        farside_dial(&c, 0); farside_fire(&c);
        r = farside_seal(&c);
        CHECK(r == 3 && c.failures == 1 && c.banner == FB_TOO_MANY, "[mech] dusk off, the check fails: %d", r);
    }

    /* [mech] frost: burning the logbooks warms the counter for nights 2-3 and costs an hour of stoking each;
     * keeping them puts last year's settings on the panel for nights 4 and 5. */
    start(&c, &p, FE_ALL, FS_FROST, 0, 0, 2);
    double h = c.hours;
    farside_answer(&c, 0);
    CHECK(farside_particles(&c) == 120 && fabs(c.hours - h - 1.0) < 1e-9 && farside_hint(&c) < 0, "[mech] frost burn: %d particles", farside_particles(&c));
    start(&c, &p, FE_ALL, FS_FROST, 1, 0, 2);
    h = c.hours;
    farside_answer(&c, 1);
    CHECK(farside_particles(&c) == 60 && fabs(c.hours - h) < 1e-9, "[mech] frost keep: %d particles", farside_particles(&c));
    start(&c, &p, FE_ALL, FS_FALL, 1, 0, 2);
    CHECK(c.night == 4, "[mech] frost keep: the fall opens after night 4");
    farside_answer(&c, 0); farside_answer(&c, 0);
    CHECK(farside_hint(&c) == 10 && fabs(farside_transmission(10) - farside_target(4)) <= farside_tol(4),
          "[mech] frost keep: logbook %d for night 5", farside_hint(&c));
    start(&c, &p, FE_ALL & ~FE_LOGBOOK, FS_FALL, 1, 0, 2);
    farside_answer(&c, 0); farside_answer(&c, 0);
    CHECK(farside_hint(&c) < 0, "[mech] frost keep without the logbook effect: no hint");

    /* [mech] radio: the institute's battery comes on top of the third night's charge. */
    start(&c, &p, FE_ALL, FS_LETTER, 1, 0, 4);
    farside_answer(&c, 0);
    double with = c.power;
    start(&c, &p, FE_ALL & ~FE_RADIO, FS_LETTER, 1, 0, 4);
    farside_answer(&c, 0);
    CHECK(fabs(with - c.power - 4.0) < 1e-9, "[mech] radio: %.2f vs %.2f", with, c.power);

    /* [mech] letter: from night 3 the line charges the battery only halfway. */
    start(&c, &p, FE_ALL, FS_LETTER, 1, 0, 4);
    CHECK(farside_charge(&c, 2) == 4.0 && farside_charge(&c, 1) == 8.0, "[mech] letter on: %.1f", farside_charge(&c, 2));
    farside_init(&c, FE_ALL & ~FE_LETTER, 4);
    CHECK(farside_charge(&c, 2) == 8.0 && farside_charge(&c, 3) == 8.0, "[mech] letter off: %.1f", farside_charge(&c, 2));

    /* [mech] fall: Agnes is gone for the fifth night (no second reading); the cells add charge and cost particles. */
    start(&c, &p, FE_ALL, FS_FALL, 1, 0, 5);
    CHECK(farside_second_reading(&c), "[mech] fall: Agnes reads until she falls");
    farside_answer(&c, 1); farside_answer(&c, 0);
    CHECK(!farside_second_reading(&c) && farside_particles(&c) == 20, "[mech] fall cells: %d particles", farside_particles(&c));
    double cells_power = c.power;
    start(&c, &p, FE_ALL, FS_FALL, 1, 0, 5);
    farside_answer(&c, 0); farside_answer(&c, 0);
    CHECK(!farside_second_reading(&c) && farside_particles(&c) == 60 && fabs(cells_power - c.power - 6.0) < 1e-9,
          "[mech] fall whole: %d particles, power %.2f vs %.2f", farside_particles(&c), c.power, cells_power);
    start(&c, &p, FE_ALL & ~FE_FALL, FS_FALL, 1, 0, 5);
    farside_answer(&c, 0); farside_answer(&c, 0);
    CHECK(farside_second_reading(&c), "[mech] fall off: Agnes still reads in night 5");

    /* [mech] dark: no charge for night 5; an empty battery waits for the sun (no failure) instead of the generator. */
    start(&c, &p, FE_ALL, FS_DARK, 1, 0, 6);
    farside_answer(&c, 0);
    CHECK(farside_charge(&c, 4) == 0.0, "[mech] dark: charge %.1f", farside_charge(&c, 4));
    c.power = 0.0; h = c.hours;
    int f0 = c.failures, r = farside_fire(&c);
    CHECK(r == 2 && c.failures == f0 && fabs(c.hours - h - 3.0) < 1e-9 && c.banner == FB_SUN, "[mech] dark: empty battery");
    start(&c, &p, FE_ALL & ~FE_DARK, FS_DARK, 1, 0, 6);
    farside_answer(&c, 0);
    c.power = 0.0; h = c.hours; f0 = c.failures; r = farside_fire(&c);
    CHECK(r == 2 && c.failures == f0 + 1 && fabs(c.hours - h - 1.0) < 1e-9 && c.banner == FB_BLACKOUT, "[mech] dark off: generator");

    /* [quiet] The heavy scenes open with no banner, and no "sealed" banner is raised before the next action. */
    int quiet[3];
    static const int heavy[] = {FS_LETTER, FS_FALL, FS_DARK};
    for (int i = 0; i < 3; i++) {
        start(&c, &p, FE_ALL, heavy[i], 1, i % 2, 8 + i);
        int opened = c.scene == heavy[i] && farside_heavy(c.scene) && c.banner == FB_NONE, loud = c.loud;
        farside_answer(&c, 0);
        if (c.scene == FS_NONE) { if (op_decide(&p, &c) == OP_SEAL) farside_seal(&c); else farside_fire(&c); }
        quiet[i] = opened && c.loud == loud && c.banner != FB_SEALED;
    }
    CHECK(quiet[0], "[quiet] letter: opens with no banner and no sealed banner follows it");
    CHECK(quiet[1], "[quiet] fall: opens with no banner (the fourth seal raises none) and no sealed banner follows it");
    CHECK(quiet[2], "[quiet] dark: opens with no banner and no sealed banner follows it");
    for (int i = 0; i < PLAY_SEEDS; i++) {
        RunResult rr;
        int frost, cells;
        play_choices(i, &frost, &cells);
        run_nights(play_key(i), FE_ALL, frost, cells, &rr);
        CHECK(rr.quiet[FS_LETTER] && rr.quiet[FS_FALL] && rr.quiet[FS_DARK], "[quiet] seed %d", i);
    }

    /* [beat] Each beat's numbers change the nights: removing them moves the mean hours or failures by 5 % or more. */
    double on_h = mean_metric(FE_ALL, DEFAULT_FROST, DEFAULT_CELLS, 0), on_f = mean_metric(FE_ALL, DEFAULT_FROST, DEFAULT_CELLS, 1);
    static const unsigned beat[] = {FE_DUSK, FE_RADIO, FE_LETTER, FE_FALL, FE_DARK};
    static const char *const beat_id[] = {"dusk", "radio", "letter", "fall", "dark"};
    for (int i = 0; i < 5; i++) {
        double bh = mean_metric(FE_ALL & ~beat[i], DEFAULT_FROST, DEFAULT_CELLS, 0), bf = mean_metric(FE_ALL & ~beat[i], DEFAULT_FROST, DEFAULT_CELLS, 1);
        CHECK(fabs(bh - on_h) >= 0.05 * on_h || fabs(bf - on_f) >= 0.05 * on_f, "[beat] %s: hours %.2f vs %.2f, failures %.2f vs %.2f",
              beat_id[i], bh, on_h, bf, on_f);
    }
    /* [choice] Each path of each choice is better on one outcome: burning wins failures, keeping wins hours;
     * the whole counter wins failures, its cells win hours. */
    double burn_h = mean_metric(FE_ALL, 0, DEFAULT_CELLS, 0), burn_f = mean_metric(FE_ALL, 0, DEFAULT_CELLS, 1);
    CHECK(burn_f < on_f && burn_h > on_h, "[choice] frost: burn %.2f h %.2f f, keep %.2f h %.2f f", burn_h, burn_f, on_h, on_f);
    double cell_h = mean_metric(FE_ALL, DEFAULT_FROST, 1, 0), cell_f = mean_metric(FE_ALL, DEFAULT_FROST, 1, 1);
    CHECK(cell_h < on_h && cell_f > on_f, "[choice] fall: cells %.2f h %.2f f, whole %.2f h %.2f f", cell_h, cell_f, on_h, on_f);

    /* [tension] Every bench run finishes. */
    int done = 0;
    for (int i = 0; i < TENSION_SEEDS; i++)
        for (int k = 0; k < TENSION_RUNS; k++) {
            RunResult rr;
            int frost, cells;
            tension_choices(k, &frost, &cells);
            run_nights(tension_key(i, k), FE_ALL, frost, cells, &rr);
            done += rr.completed;
        }
    CHECK(done == TENSION_SEEDS * TENSION_RUNS, "[tension] %d runs finished", done);

    printf("farside: %d checks, %d failures\n", checks, failures);
    return failures != 0;
}
