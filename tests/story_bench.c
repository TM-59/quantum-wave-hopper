/* Story bench: plays "The Far Side" with a human-like operator and writes
 *   docs/narrative/play-order.json  scenes in the order they opened, per seed
 *   docs/narrative/ablation.json    story numbers on/off, one beat off, each choice path
 *   docs/narrative/tension.json     per-night crises and failures, one row per run
 * Usage: story_bench <dir>          write all three files (make story)
 *        story_bench --check <dir>  re-run a few rows and require them verbatim in the files (make
 * test) story_bench --summary      print means per arm (for tuning) Every run is deterministic. */
#include "farside_run.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    uint64_t key;
    unsigned effects;
    int frost, cells;
} Job;

static RunResult run_job(Job j)
{
    RunResult r;
    run_nights(j.key, j.effects, (Choices){j.frost, j.cells}, &r);
    return r;
}

/* ------------------------------------------------------------------ rows */
static const char *ending_id(int e) { return e == 0 ? "end-whole" : e == 1 ? "end-cells" : "none"; }

static void play_row(char *buf, size_t cap, int i, const RunResult *r)
{
    const Choices picked = play_choices(i);
    int frost = picked.frost, cells = picked.cells;
    size_t n = (size_t)snprintf(
        buf, cap, "{\"seed\": \"%d\", \"choices\": {\"frost\": %d, \"fall\": %d}, \"order\": [", i,
        frost, cells);
    for (int k = 0; k < r->order_len; k++) {
        n += (size_t)snprintf(buf + n, cap - n, "%s\"%s\"", k ? ", " : "", SCENE_ID[r->order[k]]);
    }
    n += (size_t)snprintf(buf + n, cap - n, "], \"won\": %s, \"ending\": \"%s\", \"quiet\": [",
                          r->completed ? "true" : "false", ending_id(r->ending));
    int first = 1;
    for (int k = 0; k < r->order_len; k++) {
        int s = r->order[k];
        if (farside_heavy(s) && r->quiet[s]) {
            n += (size_t)snprintf(buf + n, cap - n, "%s\"%s\"", first ? "" : ", ", SCENE_ID[s]);
            first = 0;
        }
    }
    (void)snprintf(buf + n, cap - n, "]}");
}

static void ablation_row(char *buf, size_t cap, int i, const RunResult *r)
{
    (void)snprintf(
        buf, cap,
        "{\"seed\": \"%d\", \"hours\": %.2f, \"failures\": %d, \"shots\": %d, \"empties\": "
        "%d, \"red\": %d}",
        i, r->hours, r->failures, r->shots, r->blackouts + r->dark_waits, r->red);
}

static void tension_row(char *buf, size_t cap, int i, int run, const RunResult *r)
{
    const Choices picked = tension_choices(run);
    int frost = picked.frost, cells = picked.cells;
    size_t n = (size_t)snprintf(buf, cap,
                                "{\"seed\": \"%d\", \"run\": %d, \"path\": {\"frost\": \"%d\", "
                                "\"fall\": \"%d\"}, \"completed\": %s, \"crisis\": [",
                                i, run, frost, cells, r->completed ? "true" : "false");
    for (int k = 0; k < F_NIGHTS; k++) {
        n += (size_t)snprintf(buf + n, cap - n, "%s%d", k ? ", " : "",
                              r->failure[k] > 0 ? r->failure[k] : r->crisis[k]);
    }
    n += (size_t)snprintf(buf + n, cap - n, "], \"failure\": [");
    for (int k = 0; k < F_NIGHTS; k++) {
        n += (size_t)snprintf(buf + n, cap - n, "%s%d", k ? ", " : "", r->failure[k]);
    }
    (void)snprintf(buf + n, cap - n, "]}");
}

/* ------------------------------------------------------------------ arms */
enum {
    ARM_ON,
    ARM_OFF,
    ARM_DUSK,
    ARM_RADIO,
    ARM_LETTER,
    ARM_FALL,
    ARM_DARK,
    ARM_BURN,
    ARM_CELLS,
    ARMS
};
static const char *const ARM_NAME[ARMS] = {"on",   "off",  "dusk",    "radio", "letter",
                                           "fall", "dark", "frost=0", "fall=1"};
static const unsigned ARM_EFFECTS[ARMS] = {FE_ALL,
                                           0u,
                                           FE_ALL & ~FE_DUSK,
                                           FE_ALL & ~FE_RADIO,
                                           FE_ALL & ~FE_LETTER,
                                           FE_ALL & ~FE_FALL,
                                           FE_ALL & ~FE_DARK,
                                           FE_ALL,
                                           FE_ALL};
static const int ARM_FROST[ARMS] = {DEFAULT_FROST, DEFAULT_FROST, DEFAULT_FROST, DEFAULT_FROST,
                                    DEFAULT_FROST, DEFAULT_FROST, DEFAULT_FROST, 0,
                                    DEFAULT_FROST};
static const int ARM_CELL[ARMS] = {DEFAULT_CELLS, DEFAULT_CELLS, DEFAULT_CELLS,
                                   DEFAULT_CELLS, DEFAULT_CELLS, DEFAULT_CELLS,
                                   DEFAULT_CELLS, DEFAULT_CELLS, 1};

static Job ablation_job(int arm, int i)
{
    return (Job){ablation_key(i), ARM_EFFECTS[arm], ARM_FROST[arm], ARM_CELL[arm]};
}
static Job play_job(int i)
{
    const Choices picked = play_choices(i);
    int f = picked.frost, c = picked.cells;
    return (Job){play_key(i), FE_ALL, f, c};
}
static Job tension_job(int i, int run)
{
    const Choices picked = tension_choices(run);
    int f = picked.frost, c = picked.cells;
    return (Job){tension_key(i, run), FE_ALL, f, c};
}

/* ------------------------------------------------------------------ full output */
static FILE *open_out(const char *dir, const char *name)
{
    char path[512];
    (void)snprintf(path, sizeof path, "%s/%s", dir, name);
    FILE *f = fopen(path, "w");
    if (!f) {
        (void)fprintf(stderr, "cannot write %s\n", path);
        exit(1);
    }
    return f;
}

/* A short or failed write would leave a truncated table; stop with an error instead. */
static void close_out(FILE *f)
{
    int bad = ferror(f) != 0;
    if (fclose(f) != 0 || bad) {
        (void)fprintf(stderr, "write error\n");
        exit(1);
    }
}

static void write_rows(FILE *f, const char *indent, const RunResult *res, int arm)
{
    char row[256];
    for (int i = 0; i < ABLATION_SEEDS; i++) {
        ablation_row(row, sizeof row, i, &res[arm * ABLATION_SEEDS + i]);
        (void)fprintf(f, "%s%s%s\n", indent, row, i + 1 < ABLATION_SEEDS ? "," : "");
    }
}

static void write_arm_pair(FILE *f, const char *name, const RunResult *res, int off, int last)
{
    (void)fprintf(f, "    \"%s\": {\n      \"on\": [\n", name);
    write_rows(f, "        ", res, ARM_ON);
    (void)fprintf(f, "      ],\n      \"off\": [\n");
    write_rows(f, "        ", res, off);
    (void)fprintf(f, "      ]\n    }%s\n", last ? "" : ",");
}

static const char *GEN = "tests/story_bench.c (make story)";
static const char *POLICY = "An operator who reads the panel, not the code: each night it starts "
                            "at the setting written in the kept logbooks, "
                            "or at a hunch two steps either side of the nearest setting; it fires, "
                            "pools the shots fired at one setting, and seals "
                            "when the pooled share is within 0.7-1.6 x the band's half width of "
                            "the target, loosening by a quarter with every further "
                            "shot there; otherwise it turns the dial one step toward the target, "
                            "re-reads the closer of two settings that point at "
                            "each other, and never re-seals a setting the check rejected. It does "
                            "not wait for a full battery and does not know "
                            "the solver's share. Its hand comes from the run's own stirred seed, "
                            "re-drawn per night. Choices follow the path field.";

static RunResult *run_all(void)
{
    int np = PLAY_SEEDS, na = ARMS * ABLATION_SEEDS, nt = TENSION_SEEDS * TENSION_RUNS;
    RunResult *res = calloc((size_t)np + (size_t)na + (size_t)nt, sizeof *res);
    if (!res) {
        exit(1);
    }
    for (int i = 0; i < np; i++) {
        res[i] = run_job(play_job(i));
    }
    for (int a = 0; a < ARMS; a++) {
        for (int i = 0; i < ABLATION_SEEDS; i++) {
            res[np + a * ABLATION_SEEDS + i] = run_job(ablation_job(a, i));
        }
    }
    for (int i = 0; i < TENSION_SEEDS; i++) {
        for (int k = 0; k < TENSION_RUNS; k++) {
            res[np + na + i * TENSION_RUNS + k] = run_job(tension_job(i, k));
        }
    }
    return res;
}

static int summary(void)
{
    RunResult *res = run_all();
    const RunResult *ab = res + PLAY_SEEDS, *tn = res + PLAY_SEEDS + (size_t)ARMS * ABLATION_SEEDS;
    for (int a = 0; a < ARMS; a++) {
        double h = 0, f = 0, r = 0, s = 0, d = 0, b = 0, q = 0, y = 0;
        for (int i = 0; i < ABLATION_SEEDS; i++) {
            const RunResult *x = &ab[a * ABLATION_SEEDS + i];
            h += x->hours;
            f += x->failures;
            r += x->red;
            s += x->shots;
            d += x->dark_waits;
            b += x->blackouts;
            q += x->bad_seals;
            y += x->not_yet;
        }
        printf("%-8s hours %6.2f failures %5.2f (bad %4.2f black %4.2f) red %5.2f shots %6.2f dark "
               "%4.2f notyet %4.2f\n",
               ARM_NAME[a], h / ABLATION_SEEDS, f / ABLATION_SEEDS, q / ABLATION_SEEDS,
               b / ABLATION_SEEDS, r / ABLATION_SEEDS, s / ABLATION_SEEDS, d / ABLATION_SEEDS,
               y / ABLATION_SEEDS);
    }
    int nt = TENSION_SEEDS * TENSION_RUNS, zero[4] = {0}, cnt[4] = {0}, done = 0;
    double crisis[F_NIGHTS] = {0}, fails[F_NIGHTS] = {0};
    for (int i = 0; i < nt; i++) {
        int f = 0, path = (i % TENSION_RUNS) % 4;
        for (int n = 0; n < F_NIGHTS; n++) {
            f += tn[i].failure[n];
            crisis[n] += tn[i].failure[n] > 0 ? tn[i].failure[n] : tn[i].crisis[n];
            fails[n] += tn[i].failure[n];
        }
        zero[path] += f == 0;
        cnt[path]++;
        done += tn[i].completed;
    }
    printf("tension: completed %d/%d; no-failure share per path:", done, nt);
    for (int p = 0; p < 4; p++) {
        printf(" %.2f", (double)zero[p] / cnt[p]);
    }
    printf("\ncrisis per night:");
    for (int n = 0; n < F_NIGHTS; n++) {
        printf(" %.2f", crisis[n] / nt);
    }
    printf("\nfailures per night:");
    for (int n = 0; n < F_NIGHTS; n++) {
        printf(" %.2f", fails[n] / nt);
    }
    printf("\n");
    free(res);
    return 0;
}

static int write_all(const char *dir)
{
    int np = PLAY_SEEDS, na = ARMS * ABLATION_SEEDS, nt = TENSION_SEEDS * TENSION_RUNS;
    RunResult *res = run_all();
    char row[1024];

    FILE *f = open_out(dir, "play-order.json");
    (void)fprintf(
        f,
        "{\n  \"version\": 1,\n  \"generator\": \"%s\",\n  \"note\": \"Scenes open at fixed "
        "progress points: dusk before the first night and the seal "
        "that ends a night. frost 0 = burn the logbooks, 1 = keep them; fall 0 = keep the "
        "counter whole, 1 = its cells into the battery. "
        "quiet lists the heavy scenes shown with no 'sealed' banner raised before the next "
        "action. Do not edit by hand.\",\n  \"seeds\": [\n",
        GEN);
    for (int i = 0; i < np; i++) {
        play_row(row, sizeof row, i, &res[i]);
        (void)fprintf(f, "    %s%s\n", row, i + 1 < np ? "," : "");
    }
    (void)fprintf(f, "  ]\n}\n");
    close_out(f);

    const RunResult *ab = res + np;
    f = open_out(dir, "ablation.json");
    (void)fprintf(
        f,
        "{\n  \"version\": 1,\n  \"generator\": \"%s\",\n  \"note\": \"Same %d seeds in every "
        "arm. on = all story numbers, keeping the logbooks and the counter whole; "
        "off = scenes still open but every story number is neutral; beats = only that scene's "
        "numbers removed; choices = one choice fixed per path, "
        "the other at its default. The share of each particle found on the far side is the "
        "solver's in every arm. Do not edit by hand.\",\n",
        GEN, ABLATION_SEEDS);
    (void)fprintf(
        f, "  \"metrics\": {\n"
           "    \"hours\": {\"better\": \"lower\", \"kind\": \"outcome\", \"felt\": \"screen\", "
           "\"label\": \"hours the five nights took (shown on the night card): shots, seals, "
           "failed checks, the stove, the generator and waiting for the sun\"},\n"
           "    \"failures\": {\"better\": \"lower\", \"kind\": \"outcome\", \"felt\": \"screen\", "
           "\"label\": \"failures (counted on screen): a seal the check rejected\"},\n"
           "    \"shots\": {\"better\": \"lower\", \"kind\": \"outcome\", \"felt\": \"screen\", "
           "\"label\": \"shots fired over the five nights (counted on screen); each one takes a "
           "shot of battery\"},\n"
           "    \"empties\": {\"better\": \"lower\", \"kind\": \"outcome\", \"felt\": \"screen\", "
           "\"label\": \"times the battery ran empty (banner on screen): an hour on the generator "
           "while the line stood, five hours in the dark waiting for the sun after it was cut\"},\n"
           "    \"red\": {\"better\": \"lower\", \"kind\": \"resource\", \"felt\": \"resource\", "
           "\"label\": \"shots fired with the battery in the red (below 3 shots left)\"}\n"
           "  },\n  \"effects\": {\n    \"on\": [\n");
    write_rows(f, "      ", ab, ARM_ON);
    (void)fprintf(f, "    ],\n    \"off\": [\n");
    write_rows(f, "      ", ab, ARM_OFF);
    (void)fprintf(f, "    ]\n  },\n  \"beats\": {\n");
    write_arm_pair(f, "dusk", ab, ARM_DUSK, 0);
    write_arm_pair(f, "radio", ab, ARM_RADIO, 0);
    write_arm_pair(f, "letter", ab, ARM_LETTER, 0);
    write_arm_pair(f, "fall", ab, ARM_FALL, 0);
    write_arm_pair(f, "dark", ab, ARM_DARK, 1);
    (void)fprintf(f, "  },\n  \"choices\": {\n    \"frost\": {\"paths\": {\n      \"0\": [\n");
    write_rows(f, "        ", ab, ARM_BURN);
    (void)fprintf(f, "      ],\n      \"1\": [\n");
    write_rows(f, "        ", ab, ARM_ON);
    (void)fprintf(f, "      ]\n    }},\n    \"fall\": {\"paths\": {\n      \"0\": [\n");
    write_rows(f, "        ", ab, ARM_ON);
    (void)fprintf(f, "      ],\n      \"1\": [\n");
    write_rows(f, "        ", ab, ARM_CELLS);
    (void)fprintf(f, "      ]\n    }}\n  }\n}\n");
    close_out(f);

    const RunResult *tn = res + np + na;
    f = open_out(dir, "tension.json");
    (void)fprintf(f, "{\n  \"version\": 1,\n  \"generator\": \"%s\",\n  \"policy\": \"%s\",\n", GEN,
                  POLICY);
    (void)fprintf(
        f,
        "  \"crisis\": \"a shot fired with the battery in the red (below 3 shots left), an "
        "empty battery, or a failure (a failure counts as a crisis)\",\n"
        "  \"failure\": \"a seal the check rejected (the setting let too many or too few "
        "through)\",\n"
        "  \"segments\": [\"night 1: half\", \"night 2: a quarter\", \"night 3: seven in "
        "ten\", \"night 4: one in eight\", \"night 5: half, alone\"],\n"
        "  \"note\": \"%d seeds x %d runs; every run has its own stirred seed; the path comes "
        "from the run number.\",\n  \"seeds\": [\n",
        TENSION_SEEDS, TENSION_RUNS);
    for (int i = 0; i < nt; i++) {
        tension_row(row, sizeof row, i / TENSION_RUNS, i % TENSION_RUNS, &tn[i]);
        (void)fprintf(f, "    %s%s\n", row, i + 1 < nt ? "," : "");
    }
    (void)fprintf(f, "  ]\n}\n");
    close_out(f);

    double h = 0;
    for (int i = 0; i < ABLATION_SEEDS; i++) {
        h += ab[i].hours;
    }
    printf("story bench: %d runs; mean %.2f hours over five nights (default path)\n", np + na + nt,
           h / ABLATION_SEEDS);
    free(res);
    return 0;
}

/* ------------------------------------------------------------------ check mode */
static char *slurp(const char *dir, const char *name)
{
    char path[512];
    (void)snprintf(path, sizeof path, "%s/%s", dir, name);
    FILE *f = fopen(path, "rb");
    if (!f) {
        (void)fprintf(stderr, "missing %s\n", path);
        exit(1);
    }
    /* Read in growing chunks; the bench files are tens of kB. */
    size_t cap = 65536, len = 0;
    char *buf = malloc(cap);
    if (!buf) {
        exit(1);
    }
    for (;;) {
        len += fread(buf + len, 1, cap - 1 - len, f);
        if (len < cap - 1) {
            break;
        }
        char *bigger = realloc(buf, cap * 2);
        if (!bigger) {
            exit(1);
        }
        buf = bigger;
        cap *= 2;
    }
    if (ferror(f)) {
        exit(1);
    }
    buf[len] = 0;
    (void)fclose(f); /* read only: nothing is lost if closing fails */
    return buf;
}

static int check(const char *dir)
{
    int failures = 0;
    /* [seed] every operator key in the bench is distinct (paired arms share a seed by design). */
    int nk = PLAY_SEEDS + ABLATION_SEEDS + TENSION_SEEDS * TENSION_RUNS, k = 0;
    uint64_t *keys = malloc(sizeof *keys * (size_t)nk);
    if (!keys) {
        exit(1);
    }
    for (int i = 0; i < PLAY_SEEDS; i++) {
        keys[k++] = play_key(i);
    }
    for (int i = 0; i < ABLATION_SEEDS; i++) {
        keys[k++] = ablation_key(i);
    }
    for (int i = 0; i < TENSION_SEEDS; i++) {
        for (int r = 0; r < TENSION_RUNS; r++) {
            keys[k++] = tension_key(i, r);
        }
    }
    /* A pairwise look (a few hundred keys) instead of sorting with a comparator. */
    for (int i = 1; i < nk; i++) {
        for (int j = 0; j < i; j++) {
            if (keys[i] == keys[j]) {
                (void)fputs("[seed] duplicate run key\n", stderr);
                failures++;
            }
        }
    }
    free(keys);

    enum { CHECKS = 8 };
    Job jobs[CHECKS] = {play_job(0),
                        play_job(PLAY_SEEDS - 1),
                        ablation_job(ARM_ON, 0),
                        ablation_job(ARM_OFF, 0),
                        ablation_job(ARM_BURN, ABLATION_SEEDS - 1),
                        ablation_job(ARM_CELLS, ABLATION_SEEDS - 1),
                        tension_job(0, 0),
                        tension_job(TENSION_SEEDS - 1, TENSION_RUNS - 1)};
    char *play = slurp(dir, "play-order.json"), *abl = slurp(dir, "ablation.json"),
         *ten = slurp(dir, "tension.json");
    char row[1024];
    /* How many times each re-run row must appear: the default arm is reused by every beat and as
     * one path of each choice. */
    static const int copies[CHECKS] = {1, 1, 8, 1, 1, 1, 1, 1};
    for (int i = 0; i < CHECKS; i++) {
        RunResult r = run_job(jobs[i]);
        const char *hay, *file;
        if (i < 2) {
            play_row(row, sizeof row, i ? PLAY_SEEDS - 1 : 0, &r);
            hay = play;
            file = "play-order";
        } else if (i < 6) {
            ablation_row(row, sizeof row, i < 4 ? 0 : ABLATION_SEEDS - 1, &r);
            hay = abl;
            file = "ablation";
        } else {
            tension_row(row, sizeof row, i == 6 ? 0 : TENSION_SEEDS - 1,
                        i == 6 ? 0 : TENSION_RUNS - 1, &r);
            hay = ten;
            file = "tension";
        }
        int found = 0;
        for (const char *c = hay; (c = strstr(c, row)); c++) {
            found++;
        }
        if (found != copies[i]) {
            (void)fprintf(stderr, "[bench] %s.json does not match the code (%d of %d copies): %s\n",
                          file, found, copies[i], row);
            failures++;
        }
    }
    free(play);
    free(abl);
    free(ten);
    if (failures) {
        (void)fprintf(stderr, "story bench check: %d failure(s); run make story to regenerate\n",
                      failures);
        return 1;
    }
    puts("story bench check: 8 re-run rows match docs/narrative; all run seeds distinct");
    return 0;
}

int main(int argc, char **argv)
{
    if (argc == 3 && strcmp(argv[1], "--check") == 0) {
        return check(argv[2]);
    }
    if (argc == 2 && strcmp(argv[1], "--summary") == 0) {
        return summary();
    }
    if (argc == 2) {
        return write_all(argv[1]);
    }
    (void)fputs("usage: story_bench <dir> | story_bench --check <dir> | story_bench --summary\n",
                stderr);
    return 2;
}
