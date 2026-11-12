#include "farside.h"
#include "observer.h"

#include <math.h>
#include <string.h>

#define STEP_H 0.05            /* barrier height per dial step */
#define SHOT_HOURS 0.05        /* a shot: the packet and the tally */
#define PARTICLE_HOURS 0.003   /* plus this per particle sent (the counter resets after each one) */
#define SETTLE_HOURS 0.15      /* plus the copper settling, up to this long after a shot (drawn per shot) */
#define SEAL_HOURS 0.25        /* writing a seal into the night's record */
#define BAD_HOURS 0.5          /* a failed seal: the check, tearing the page out */
#define RED_LINE 2.0           /* a shot that leaves less than this in the battery is fired in the red */
#define BASE_CHARGE 8.0        /* at dusk the line charges the battery up to this many shots (never drains it) */
#define START_POWER 0.0        /* the battery at the first dusk, before the line charges it */
#define GEN_WAIT 1.0           /* an empty battery before the line is cut: an hour on the generator */
#define GEN_POWER 3.0
#define SUN_WAIT 5.0           /* an empty battery after the line is cut: sit in the dark until the sun is on the panels */
#define SUN_POWER 3.0
#define BASE_PARTICLES 12      /* the dome's own counter */

#define DUSK_PARTICLES 60      /* FE_DUSK: Agnes's copper counter */
#define STOVE_FACTOR 3         /* FE_STOVE: a cold counter in nights 2-3 hears a third of the particles */
#define STOVE_HOURS 1.0        /* FE_STOVE: an hour of stoking before the counter stops drifting */
#define RADIO_BATTERY 4.0      /* FE_RADIO: the institute's battery, on top of the charge of night 3 */
#define LETTER_CHARGE 4.0      /* FE_LETTER: the line charges only up to this from night 3 */
#define CELL_POWER 6.0         /* FE_CELLS: the counter's cells, added at the start of night 5 */
#define CELL_FACTOR 3          /* FE_CELLS: without its cells the counter hears a third of the particles */
#define RADIO_SETTLE 0.5       /* FE_RADIO: the battery runs the heater in night 3; the copper settles in half the time */
#define LETTER_SETTLE 2.0      /* FE_LETTER: the line at half voltage from night 3; the copper takes twice as long to settle */
#define DARK_SETTLE 2.0        /* FE_DARK: no heater in night 5; twice as long again */
#define DARK_DRAIN 0.25        /* FE_DARK: the cold takes three quarters of the battery before the fifth night starts */

/* Each night's band of far-side share: Agnes's targets. */
static const double TARGET[F_NIGHTS] = {0.50, 0.25, 0.70, 0.13, 0.50};
static const double TOL[F_NIGHTS] = {0.08, 0.04, 0.05, 0.04, 0.03};
/* Last year's settings, written in the logbooks (FE_LOGBOOK), for nights 4 and 5. */
static const int LOGBOOK[F_NIGHTS] = {-1, -1, -1, 17, 10};

/* The letter, Agnes's fall and the dark get the screen to themselves: no "sealed" banner. */
static const int HEAVY[FS_COUNT] = {0, 0, 0, 1, 1, 1, 0};

int farside_options(int scene) { return scene == FS_FROST || scene == FS_FALL ? 2 : 1; }
int farside_heavy(int scene) { return scene >= 0 && scene < FS_COUNT && HEAVY[scene]; }
double farside_target(int night) { return night >= 0 && night < F_NIGHTS ? TARGET[night] : 0.0; }
double farside_tol(int night) { return night >= 0 && night < F_NIGHTS ? TOL[night] : 0.0; }

int farside_ending(const Farside *c)
{
    return c->scene == FS_DAWN || c->night >= F_NIGHTS ? c->cells : -1;
}

static int on(const Farside *c, unsigned effect) { return (c->effects & effect) != 0; }

/* ------------------------------------------------------------------ the solver's share, one run per dial step */
static double share[F_STEPS];
static int share_ready[F_STEPS];

double farside_transmission(int step)
{
    if (step < 0) step = 0;
    if (step >= F_STEPS) step = F_STEPS - 1;
    if (!share_ready[step]) {
        /* The observer's own run: the same grid, packet and barrier (left edge 0, width 2) as the observatory. */
        qw_reset(0);
        qw_controls(0.0, step * STEP_H);
        qw_launch();
        while (!qw_value(QW_DONE)) qw_step(400);
        share[step] = qw_value(QW_P_RIGHT);
        share_ready[step] = 1;
    }
    return share[step];
}

/* ------------------------------------------------------------------ the counter */
static uint64_t mix64(uint64_t z)
{
    z = (z ^ (z >> 30)) * 0xbf58476d1ce4e5b9ULL;
    z = (z ^ (z >> 27)) * 0x94d049bb133111ebULL;
    return z ^ (z >> 31);
}

static double to_unit(uint64_t z) { return (double)(mix64(z) >> 11) / 9007199254740992.0; }

/* Draws are addressed, not streamed: the i-th particle sent at one dial step in one night always gets the same draw,
 * whatever happened before. Two runs that reach the same setting see the same particles (common random numbers). */
static double draw(const Farside *c, uint64_t kind, uint64_t a, uint64_t b)
{
    uint64_t z = mix64(c->seed + kind * 0x632be59bd9b4e019ULL);
    z = mix64(z + (uint64_t)(c->night + 1) * 0x9e3779b97f4a7c15ULL);
    z = mix64(z + (a + 1u) * 0xd1b54a32d192ed03ULL);
    return to_unit(z + (b + 1u) * 0x8cb92ba72f3d8dd7ULL);
}

double farside_settle(const Farside *c)
{
    double f = 1.0;
    if (c->night == 2 && on(c, FE_RADIO)) f *= RADIO_SETTLE;
    if (c->night >= 2 && on(c, FE_LETTER)) f *= LETTER_SETTLE;
    if (c->night == 4 && on(c, FE_DARK)) f *= DARK_SETTLE;
    return f;
}

int farside_particles(const Farside *c)
{
    int n = on(c, FE_DUSK) ? DUSK_PARTICLES : BASE_PARTICLES;
    /* The frost: a cold counter misses particles in nights 2-3 unless the stove warms it. */
    if (c->frost == 1 && on(c, FE_STOVE) && (c->night == 1 || c->night == 2)) n /= STOVE_FACTOR;
    if (c->cells == 1 && on(c, FE_CELLS) && c->night == 4) n /= CELL_FACTOR;
    return n;
}

int farside_hint(const Farside *c)
{
    if (c->frost == 1 && on(c, FE_LOGBOOK) && c->night < F_NIGHTS) return LOGBOOK[c->night];
    return -1;
}

int farside_second_reading(const Farside *c)
{
    return on(c, FE_DUSK) && c->agnes;
}

/* Each particle is found on the far side with the solver's probability (Born's rule), one draw per particle. */
static int count(Farside *c, int n)
{
    double p = farside_transmission(c->dial);
    int k = 0;
    for (int i = 0; i < n; i++) k += draw(c, 1, (uint64_t)c->dial, (uint64_t)(c->tally_n[c->dial] + i)) < p;
    c->tally_n[c->dial] += n;
    c->tally_k[c->dial] += k;
    c->last_n = n;
    c->last_k = k;
    return k;
}

/* ------------------------------------------------------------------ nights and scenes */
static void open_scene(Farside *c, int scene)
{
    c->scene = scene;
    if (c->order_len < FS_COUNT) c->order[c->order_len++] = scene;
    if (farside_heavy(scene)) c->banner = FB_NONE;
}

double farside_charge(const Farside *c, int night)
{
    if (night == 4 && on(c, FE_DARK)) return 0.0;                 /* the line is cut */
    if (night >= 2 && on(c, FE_LETTER)) return LETTER_CHARGE;     /* the ration */
    return BASE_CHARGE;
}

static void start_night(Farside *c)
{
    memset(c->tally_n, 0, sizeof c->tally_n);
    memset(c->tally_k, 0, sizeof c->tally_k);
    memset(c->ruled, 0, sizeof c->ruled);
    memset(c->doubt, 0, sizeof c->doubt);
    c->last_n = c->last_k = 0;
    if (c->night == 4 && on(c, FE_DARK)) c->power *= DARK_DRAIN;
    c->power = fmax(c->power, farside_charge(c, c->night));
    if (c->night == 2 && on(c, FE_RADIO)) c->power += RADIO_BATTERY;
    if (c->night == 4 && c->cells == 1 && on(c, FE_CELLS)) c->power += CELL_POWER;
    if (c->frost == 0 && on(c, FE_STOVE) && (c->night == 1 || c->night == 2)) c->hours += STOVE_HOURS;
}

void farside_init(Farside *c, unsigned effects, uint64_t seed)
{
    memset(c, 0, sizeof *c);
    c->effects = effects;
    c->seed = seed;
    c->frost = c->cells = -1;
    c->agnes = 1;
    c->power = START_POWER;
    c->dial = 12;
    c->scene = FS_NONE;
    open_scene(c, FS_DUSK);
}

static void clear_night(Farside *c)
{
    static const int next[F_NIGHTS] = {FS_FROST, FS_RADIO, FS_NONE, FS_FALL, FS_DAWN};
    int scene = next[c->night];
    c->night++;
    if (scene == FS_NONE) {
        c->banner = FB_SEALED;
        c->loud++;
        start_night(c);
        return;
    }
    /* The heavy scenes and the ending get the screen to themselves: no "sealed" banner. */
    if (!farside_heavy(scene) && scene != FS_DAWN) { c->banner = FB_SEALED; c->loud++; }
    else c->banner = FB_NONE;
    open_scene(c, scene);
}

static void fail(Farside *c, int banner)
{
    c->failures++;
    c->fail_night[c->night]++;
    c->crisis_night[c->night] = 1;
    c->banner = banner;
}

static int playing(const Farside *c) { return c->scene == FS_NONE && c->night < F_NIGHTS; }

void farside_dial(Farside *c, int step)
{
    if (!playing(c)) return;
    c->dial = step < 0 ? 0 : step >= F_STEPS ? F_STEPS - 1 : step;
}

int farside_fire(Farside *c)
{
    if (!playing(c)) return 1;
    c->banner = FB_NONE;
    if (c->power < 1.0 - 1e-9) {
        if (c->night == 4 && on(c, FE_DARK)) {
            /* No line and no generator fuel left: sit in the dark until the sun reaches the panels. */
            c->hours += SUN_WAIT;
            c->power = SUN_POWER;
            c->dark_waits++;
            c->crisis_night[c->night] = 1;
            c->banner = FB_SUN;
        } else {
            c->hours += GEN_WAIT;
            c->power = GEN_POWER;
            c->blackouts++;
            c->crisis_night[c->night] = 1;
            c->banner = FB_BLACKOUT;
        }
        return 2;
    }
    c->power -= 1.0;
    c->shots++;
    if (c->power < RED_LINE - 1e-9) { c->red++; c->crisis_night[c->night] = 1; }
    int n = farside_particles(c);
    count(c, n);
    c->hours += SHOT_HOURS + PARTICLE_HOURS * n + SETTLE_HOURS * farside_settle(c) * draw(c, 2, (uint64_t)c->shots, 0);
    return 0;
}

int farside_seal(Farside *c)
{
    if (!playing(c) || c->tally_n[c->dial] == 0) return 1;
    int night = c->night, step = c->dial;
    double lo = TARGET[night] - TOL[night], hi = TARGET[night] + TOL[night];
    c->hours += SEAL_HOURS;
    if (farside_second_reading(c)) {
        /* Agnes taps the counter twice and reads it again (no battery: her counter's own cell). */
        int n = farside_particles(c);
        count(c, n);
        c->hours += PARTICLE_HOURS * n;
        double p = (double)c->tally_k[step] / c->tally_n[step];
        if (p < lo || p > hi) {
            /* A near miss: a crisis, not a failure. */
            c->doubt[step]++;
            c->not_yet++;
            c->crisis_night[night] = 1;
            c->banner = FB_NOT_YET;
            return 2;
        }
    }
    double t = farside_transmission(step);
    if (t >= lo && t <= hi) { clear_night(c); return 0; }
    c->hours += BAD_HOURS;
    c->ruled[step] = t > hi ? 1 : -1;
    c->bad_seals++;
    fail(c, t > hi ? FB_TOO_MANY : FB_TOO_FEW);
    return 3;
}

int farside_answer(Farside *c, int option)
{
    int scene = c->scene;
    if (scene == FS_NONE || option < 0 || option >= farside_options(scene)) return 1;
    c->scene = FS_NONE;
    switch (scene) {
    case FS_DUSK:
        start_night(c);
        break;
    case FS_FROST:
        c->frost = option;
        start_night(c);
        break;
    case FS_RADIO:
        open_scene(c, FS_LETTER);
        break;
    case FS_LETTER:
        start_night(c);
        break;
    case FS_FALL:
        c->cells = option;
        if (on(c, FE_FALL)) c->agnes = 0;
        open_scene(c, FS_DARK);
        break;
    case FS_DARK:
        start_night(c);
        break;
    case FS_DAWN:
        break;
    default:
        return 1;
    }
    return 0;
}

double farside_value(const Farside *c, int field)
{
    int night = c->night < F_NIGHTS ? c->night : F_NIGHTS - 1;
    switch (field) {
    case FV_NIGHT: return c->night;
    case FV_SCENE: return c->scene;
    case FV_POWER: return c->power;
    case FV_HOURS: return c->hours;
    case FV_SHOTS: return c->shots;
    case FV_RED: return c->red;
    case FV_FAILURES: return c->failures;
    case FV_DIAL: return c->dial;
    case FV_HEIGHT: return c->dial * STEP_H;
    case FV_PARTICLES: return farside_particles(c);
    case FV_LAST_N: return c->last_n;
    case FV_LAST_K: return c->last_k;
    case FV_POOL_N: return c->tally_n[c->dial];
    case FV_POOL_K: return c->tally_k[c->dial];
    case FV_TARGET: return TARGET[night];
    case FV_TOL: return TOL[night];
    case FV_HINT: return farside_hint(c);
    case FV_AGNES: return farside_second_reading(c);
    case FV_BANNER: return c->banner;
    case FV_LOUD: return c->loud;
    case FV_HEAVY: return farside_heavy(c->scene);
    case FV_ENDING: return farside_ending(c);
    case FV_FROST: return c->frost;
    case FV_CELLS: return c->cells;
    case FV_RED_LINE: return RED_LINE;
    case FV_RULED: return c->ruled[c->dial];
    case FV_LINE: return !(c->night >= 4 && on(c, FE_DARK) && c->scene != FS_FALL);
    case FV_CHARGE: return farside_charge(c, night);
    case FV_DARK_WAITS: return c->dark_waits;
    default: return 0.0;
    }
}
