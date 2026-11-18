// The read-only hooks web/app.mjs puts on window once the solver has loaded; scripts/smoke.cjs reads them
// in the page (it waits for qwView before it reads anything else).
import type { Gate, QwView } from './app.mjs';
import type { startResonance } from './resonance.mjs';
import type { startSlits } from './slits.mjs';

declare global {
  interface Window {
    qwView: QwView;
    rsView: ReturnType<typeof startResonance>;
    slView: ReturnType<typeof startSlits>;
    __tmGate: Gate;
  }
}
