#ifndef FARSIDE_POLICY_H
#define FARSIDE_POLICY_H

/* Shared by the story test and the story bench: seeds and a human-like operator. */
#include "../src/farside.h"

#include <math.h>
#include <stdint.h>
#include <string.h>

static inline uint64_t mix64(uint64_t z)
{
    z = (z ^ (z >> 30)) * 0xbf58476d1ce4e5b9ULL;
    z = (z ^ (z >> 27)) * 0x94d049bb133111ebULL;
    return z ^ (z >> 31);
}

/* Seed and index are stirred separately before they are combined (no XOR or sums of raw keys). */
static inline uint64_t sub_seed(uint64_t seed, uint64_t index, uint64_t salt)
{
    return mix64(mix64(seed + salt * 0x632be59bd9b4e019ULL) + (index + 1u) * 0x9e3779b97f4a7c15ULL);
}

typedef struct { uint64_t s; } Rng;
static inline double rng_unit(Rng *r)
{
    r->s += 0x9e3779b97f4a7c15ULL;
    return (double)(mix64(r->s) >> 11) / 9007199254740992.0;
}

/* An operator who reads the panel, not the code. Each night it starts at the logbook's setting when there is one,
 * otherwise at a hunch: the dial step nearest the band by its sense of the curve, off by -2..+2 steps. It fires,
 * pools every shot fired at the same setting tonight, and seals when the pooled share is within its own margin of the
 * target (0.7-1.6 x the band's half width, loosening by a quarter with every further shot at that setting: it gets
 * impatient). Otherwise it turns the dial one step toward the target (a higher barrier lets fewer through). Caught
 * between two settings that each point at the other, it fires again at the one it has read less. A setting the
 * check rejected is never sealed again tonight; it moves away from it the way the check said. Once Agnes has said
 * "not yet" at a setting, it treats the reading there as off target while it stays outside her band. */
enum { OP_FIRE, OP_SEAL };

typedef struct {
    Rng rng;
    uint64_t key;
    int night;
    double margin;
    int hunch;
} Operator;

static inline void op_init(Operator *p, uint64_t key)
{
    memset(p, 0, sizeof *p);
    p->key = key;
    p->night = -1;
}

/* The dial step whose share lies nearest the target: the operator's sense of the curve (it has seen the observatory). */
static inline int op_nearest(int night)
{
    int best = 0;
    double target = farside_target(night), gap = 2.0;
    for (int s = 0; s < F_STEPS; s++) {
        double g = fabs(farside_transmission(s) - target);
        if (g < gap) { gap = g; best = s; }
    }
    return best;
}

static inline double op_read(const Farside *c, int s)
{
    return c->tally_n[s] ? (double)c->tally_k[s] / c->tally_n[s] : -1.0;
}

/* Sets the dial and returns OP_FIRE or OP_SEAL. */
static inline int op_decide(Operator *p, Farside *c)
{
    int night = c->night;
    if (night != p->night) {
        p->night = night;
        p->rng.s = sub_seed(p->key, (uint64_t)night, 5);
        p->margin = 0.7 + 0.9 * rng_unit(&p->rng);
        int hint = farside_hint(c);
        p->hunch = hint >= 0 ? hint : op_nearest(night) + (int)(5.0 * rng_unit(&p->rng)) - 2;
        if (p->hunch < 0) p->hunch = 0;
        if (p->hunch >= F_STEPS) p->hunch = F_STEPS - 1;
        farside_dial(c, p->hunch);
    }
    double target = farside_target(night), tol = farside_tol(night);
    for (int guard = 0; guard < F_STEPS; guard++) {
        int s = c->dial;
        if (c->ruled[s]) {           /* the check rejected this setting: away from it */
            farside_dial(c, s + c->ruled[s]);
            if (c->dial == s) return OP_FIRE;
            continue;
        }
        double r = op_read(c, s);
        if (r < 0.0) return OP_FIRE;
        int shots = (c->tally_n[s] + farside_particles(c) - 1) / farside_particles(c);
        double margin = tol * p->margin * (1.0 + 0.25 * (shots - 1));
        /* After Agnes has said "not yet" here, it does not seal this setting again while the reading is outside her band. */
        int doubted = c->doubt[s] && farside_second_reading(c) && fabs(r - target) > tol;
        if (fabs(r - target) <= margin && !doubted) return OP_SEAL;
        int next = s + (r > target ? 1 : -1);
        if (next < 0 || next >= F_STEPS) return OP_FIRE;
        double rn = op_read(c, next);
        if (c->ruled[next] || (rn >= 0.0 && (rn > target) != (r > target))) {
            /* Caught between two settings: read again the one read less (then the one that looks closer). */
            if (!c->ruled[next] && (c->tally_n[next] < c->tally_n[s] ||
                                    (c->tally_n[next] == c->tally_n[s] && fabs(rn - target) < fabs(r - target))))
                farside_dial(c, next);
            return OP_FIRE;
        }
        farside_dial(c, next);
        if (rn < 0.0) return OP_FIRE;
    }
    return OP_FIRE;
}

#endif
