// Types of dist/physics.mjs, the Emscripten build of src/*.c (`make web`). The functions are the
// -sEXPORTED_FUNCTIONS list of the Makefile; each follows its C prototype in src/*.h.
export interface WaveEngine {
  HEAPU8: Uint8Array;
  _qw_reset(mode: number): number;
  _qw_controls(left: number, height: number): void;
  _qw_launch(): void;
  _qw_step(count: number): void;
  _qw_value(field: number): number;
  _qw_density(bin: number): number;
  _qw_real(bin: number): number;
  _qw_half_height(): number;
  _fw_reset(seed: number): void;
  _fw_dial(step: number): void;
  _fw_fire(): number;
  _fw_seal(): number;
  _fw_answer(option: number): number;
  _fw_value(field: number): number;
  _fw_share(step: number): number;
  _rs_reset(mode: number): number;
  _rs_energy(next: number): void;
  _rs_launch(): void;
  _rs_step(count: number): void;
  _rs_value(field: number): number;
  _rs_density(bin: number): number;
  _rs_zoom(site: number): number;
  _rs_curve(point: number): number;
  _rs_curve_packet(point: number): number;
  _sl_reset(mode: number): number;
  _sl_controls(lambda: number, sep: number, width: number): void;
  _sl_scramble(phi: number): void;
  _sl_seed(next: number): void;
  _sl_launch(): void;
  _sl_step(count: number): void;
  _sl_measure(): void;
  _sl_value(field: number): number;
  /** Address of the RGBA image in HEAPU8. */
  _sl_image(): number;
  _sl_screen(bin: number): number;
  _sl_screen_theory(bin: number): number;
  _sl_screen_ideal(bin: number): number;
  _sl_particle(n: number): number;
}

export default function createWave(): Promise<WaveEngine>;
