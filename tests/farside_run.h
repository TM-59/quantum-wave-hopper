#ifndef FARSIDE_RUN_H
#define FARSIDE_RUN_H

/* Five whole nights with the human-like operator; shared by the story test and bench. */
#include "farside_policy.h"

#include <string.h>

enum { RUN_GUARD_ACTIONS = 3000 };  /* counted as "did not finish" */

typedef struct {
    double hours;
    int shots, red, failures, dark_waits, blackouts, bad_seals, not_yet, completed, ending;
    int crisis[F_NIGHTS], failure[F_NIGHTS];
    int order[FS_COUNT], order_len;
    int quiet[FS_COUNT];  /* heavy scene shown with no "sealed" banner, before or after it */
} RunResult;

static const char *const SCENE_ID[FS_COUNT] = {"dusk", "frost", "radio", "letter", "fall", "dark", "dawn"};

static inline void run_nights(uint64_t key, unsigned effects, int frost, int cells, RunResult *out)
{
    Farside c;
    Operator p;
    memset(out, 0, sizeof *out);
    out->ending = -1;
    farside_init(&c, effects, sub_seed(key, 0, 9));
    op_init(&p, key);
    int actions = 0;
    while (c.night < F_NIGHTS && actions < RUN_GUARD_ACTIONS) {
        if (c.scene != FS_NONE) {
            int scene = c.scene, loud = c.loud, calm = c.banner == FB_NONE;
            int option = scene == FS_FROST ? frost : scene == FS_FALL ? cells : 0;
            farside_answer(&c, option);
            if (farside_heavy(scene)) {
                if (c.scene == FS_NONE) {   /* the first action after the scene closes */
                    if (op_decide(&p, &c) == OP_SEAL) farside_seal(&c); else farside_fire(&c);
                    actions++;
                }
                out->quiet[scene] = calm && c.loud == loud && c.banner != FB_SEALED;
            }
            continue;
        }
        if (op_decide(&p, &c) == OP_SEAL) farside_seal(&c); else farside_fire(&c);
        actions++;
    }
    out->completed = c.night >= F_NIGHTS;
    out->hours = c.hours;
    out->shots = c.shots;
    out->red = c.red;
    out->failures = c.failures;
    out->dark_waits = c.dark_waits;
    out->blackouts = c.blackouts;
    out->bad_seals = c.bad_seals;
    out->not_yet = c.not_yet;
    out->ending = farside_ending(&c);
    for (int n = 0; n < F_NIGHTS; n++) { out->crisis[n] = c.crisis_night[n]; out->failure[n] = c.fail_night[n]; }
    out->order_len = c.order_len;
    memcpy(out->order, c.order, sizeof c.order);
}

/* Seeds: every row and run gets its own stirred key (no XOR or sums of raw keys). */
#define STORY_BASE 20261113ULL
enum { SALT_SEED = 1, SALT_PLAY = 2, SALT_ABLATION = 3, SALT_TENSION = 4 };
static inline uint64_t story_seed(int i) { return sub_seed(STORY_BASE, (uint64_t)i, SALT_SEED); }
static inline uint64_t play_key(int i) { return sub_seed(story_seed(i), 0, SALT_PLAY); }
static inline uint64_t ablation_key(int i) { return sub_seed(story_seed(i), 0, SALT_ABLATION); }
static inline uint64_t tension_key(int i, int run) { return sub_seed(story_seed(i), (uint64_t)run, SALT_TENSION); }

enum { PLAY_SEEDS = 12, ABLATION_SEEDS = 40, TENSION_SEEDS = 16, TENSION_RUNS = 12 };
/* Default choices for the ablation rows: keep the logbooks, keep the counter whole. */
enum { DEFAULT_FROST = 1, DEFAULT_CELLS = 0 };
static inline void play_choices(int i, int *frost, int *cells) { *frost = i % 2; *cells = (i / 2) % 2; }
static inline void tension_choices(int run, int *frost, int *cells) { *frost = run % 2; *cells = (run / 2) % 2; }

#endif
