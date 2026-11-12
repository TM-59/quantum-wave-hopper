#ifndef FARSIDE_H
#define FARSIDE_H

#include <stdint.h>

/* "The Far Side": a story layer over the unchanged solver in wave.c, read through the observer (observer.c).
 * The ridge observatory loses its power line at dawn after the fifth night. Agnes, its last keeper, asks Hop for five
 * calibrations of the Wall: each night a barrier height whose far-side share lands inside a band she names.
 * Hop turns the dial (barrier height, 0 to 1.5 in steps of 0.05), fires shots and seals a setting.
 * A shot sends a number of single particles at the barrier. Each one is found on the far side with the probability
 * the solver computes for that height: P(x > barrier) after the observer's full run (qw_value(QW_P_RIGHT)), the same
 * number the observatory shows. The story never changes that probability. It changes only how many particles a shot
 * sends, the battery, the hours a night takes, whether Agnes reads the counter again before a seal, and what an empty
 * battery costs. A seal is checked against the solver's share: outside the band it fails (a failure).
 * An empty battery costs hours (the generator, or after the line is cut the sun on the panels), not a failure. */

enum { F_NIGHTS = 5, F_STEPS = 31 };   /* dial steps 0..30, height = step x 0.05 */

enum {
    FS_NONE = -1,
    FS_DUSK,     /* Agnes at the counter: the line is cut after the fifth night; her copper counter; she reads every seal twice */
    FS_FROST,    /* frost on the counter; choice: burn the old logbooks in the stove, or keep them */
    FS_RADIO,    /* the institute on the radio: the numbers are good, a battery is on its way (the false peak) */
    FS_LETTER,   /* the letter: the institute wanted the counter, not the dome; the line company cuts the ration (heavy) */
    FS_FALL,     /* Agnes falls on the ice; the ambulance takes her down; choice: the counter whole or its cells (heavy) */
    FS_DARK,     /* the line is cut; one battery; nobody reads the counter twice (heavy) */
    FS_DAWN,     /* dawn; ending */
    FS_COUNT
};

/* Numeric story effects, one bit each, so the bench can remove them one by one. */
enum {
    FE_DUSK = 1u << 0,     /* Agnes's copper counter (more particles per shot) and her second reading before every seal */
    FE_STOVE = 1u << 1,    /* the frost: a cold counter hears half the particles in nights 2-3; burning the logbooks warms it, an hour of stoking each */
    FE_LOGBOOK = 1u << 2,  /* keeping the logbooks: last year's setting for nights 4 and 5 is written in them */
    FE_RADIO = 1u << 3,    /* the institute's battery */
    FE_LETTER = 1u << 4,   /* the line company cuts the nightly charge from night 3 */
    FE_FALL = 1u << 5,     /* Agnes is gone for night 5: no second reading */
    FE_CELLS = 1u << 6,    /* the counter's cells into the battery for night 5 (more charge, half the particles) */
    FE_DARK = 1u << 7,     /* the line is cut for night 5: no charge, an empty battery waits for the sun on the panels */
    FE_ALL = (1u << 8) - 1u
};

enum { FB_NONE, FB_SEALED, FB_NOT_YET, FB_TOO_MANY, FB_TOO_FEW, FB_BLACKOUT, FB_SUN };

typedef struct {
    unsigned effects;
    uint64_t seed;        /* the counter's draws are addressed by (seed, night, dial step, particle number) */
    int night;            /* 0..4; F_NIGHTS when the five nights are over */
    int scene;            /* FS_* open now, or FS_NONE */
    int frost;            /* -1 until chosen; 0 = burn the logbooks, 1 = keep them */
    int cells;            /* -1 until chosen; 0 = keep the counter whole, 1 = its cells into the battery */
    int agnes;            /* 1 while Agnes is in the dome */
    int dial;             /* barrier height in dial steps */
    double power;         /* battery, in shots */
    double hours;         /* hours the five nights have taken so far */
    int shots, red, failures, dark_waits, blackouts, bad_seals, not_yet;
    int fail_night[F_NIGHTS];
    int crisis_night[F_NIGHTS];   /* 1 if a shot was fired in the red, Agnes said "not yet", a seal failed or the battery ran out */
    int tally_n[F_STEPS], tally_k[F_STEPS];   /* this night's particles sent / found on the far side, per dial step */
    int ruled[F_STEPS];   /* this night's settings the check rejected: +1 too many through, -1 too few */
    int doubt[F_STEPS];   /* this night's "not yet" from Agnes, per dial step */
    int last_n, last_k;   /* the last shot */
    int banner;           /* FB_* shown on screen until the next action */
    int loud;             /* number of "sealed" banners raised */
    int order[FS_COUNT];
    int order_len;
} Farside;

enum FarsideValue {
    FV_NIGHT, FV_SCENE, FV_POWER, FV_HOURS, FV_SHOTS, FV_RED, FV_FAILURES, FV_DIAL, FV_HEIGHT, FV_PARTICLES,
    FV_LAST_N, FV_LAST_K, FV_POOL_N, FV_POOL_K, FV_TARGET, FV_TOL, FV_HINT, FV_AGNES, FV_BANNER, FV_LOUD, FV_HEAVY,
    FV_ENDING, FV_FROST, FV_CELLS, FV_RED_LINE, FV_RULED, FV_LINE, FV_CHARGE, FV_DARK_WAITS, FV_FIELDS
};

void farside_init(Farside *c, unsigned effects, uint64_t seed);
/* Turn the dial to a step (clamped). Does nothing while a scene is open or after the end. */
void farside_dial(Farside *c, int step);
/* Fire one shot at the current dial. Returns 0 when particles were sent, 1 when nothing happened, 2 when the battery
 * was empty (a blackout or a wait for the sun instead of a shot). */
int farside_fire(Farside *c);
/* Seal the current dial. Needs a reading at this dial tonight. Returns 0 sealed, 1 refused, 2 Agnes said not yet,
 * 3 the check failed. */
int farside_seal(Farside *c);
/* Close the open scene. Choice scenes take option 0 or 1; others take 0. Returns 0 when accepted. */
int farside_answer(Farside *c, int option);
int farside_options(int scene);
int farside_heavy(int scene);
/* 0 = the counter whole, 1 = its cells used; -1 before dawn. */
int farside_ending(const Farside *c);
/* Particles a shot sends right now. */
int farside_particles(const Farside *c);
/* The share of each particle found on the far side at a dial step: the solver's P(x > barrier) after the observer's
 * full run. The same for every story effect. */
double farside_transmission(int step);
/* Dial step written in the kept logbooks for this night, or -1. */
int farside_hint(const Farside *c);
double farside_target(int night);
double farside_tol(int night);
/* 1 while Agnes reads the counter a second time before a seal. */
int farside_second_reading(const Farside *c);
/* How long the copper takes to settle after a shot, relative to a warm dome on a full line. */
double farside_settle(const Farside *c);
/* Charge the battery gets at the start of a night (shots). */
double farside_charge(const Farside *c, int night);
double farside_value(const Farside *c, int field);

#endif
