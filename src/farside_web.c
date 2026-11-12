/* WASM bridge for the story layer: one Farside, read through plain doubles. */
#include "farside.h"

static Farside story;

void fw_reset(double seed) { farside_init(&story, FE_ALL, (uint64_t)seed); }
void fw_dial(int step) { farside_dial(&story, step); }
int fw_fire(void) { return farside_fire(&story); }
int fw_seal(void) { return farside_seal(&story); }
int fw_answer(int option) { return farside_answer(&story, option); }
double fw_value(int field) { return farside_value(&story, field); }
double fw_share(int step) { return farside_transmission(step); }
