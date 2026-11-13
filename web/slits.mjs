// The double-slit experiment (M3): the C adapter src/slits.c evolves the 2D wave (wave2d.c), paints the density,
// measures the far-field screen and draws the particles by its counter; this module only reads numbers and draws.
const $ = id => document.getElementById(id);
const FIELDS = ['time','mode','lambda','k','sep','width','scramble','coherence','norm','pBeyond','done','tMax','steps','nx','ny',
  'x0','y0','dx','wallX','wallT','screenL','screenHalf','bins','spacing','dky','visibility','visibilityTheory','spacingMeasured','measuredAt','seed'];
const STEPS_PER_TICK = 3, TICK = 1 / 60, MEASURE_EVERY = 15, PARTICLES_PER_TICK = 25, PARTICLES_MAX = 6000;
const MODES = ['BOTH SLITS OPEN', 'SLIT B CLOSED', 'SLIT A CLOSED', 'WATCHING WHICH SLIT'];
const COLOURS = {measured: '#56b4e9', theory: '#f0e4c8', ideal: '#aab3c9', particle: '#e69f00', wall: '#f0e4c8'};
// A visual jitter of a particle across the detector strip: a fixed hash of its number (decoration, not physics).
const jitter = n => { let z = (n + 1) * 2654435761 >>> 0; z ^= z >>> 15; z = Math.imul(z, 2246822519) >>> 0; z ^= z >>> 13; return ((z >>> 0) % 1000) / 1000; };

export function startSlits(engine, textPx) {
  let state, screen = [], theory = [], ideal = [], particles = [], paused = true, last = 0, accumulator = 0, ticks = 0, firing = false;
  let texts = [];
  const offscreen = document.createElement('canvas'); offscreen.width = 256; offscreen.height = 256;
  const read = () => Object.fromEntries(FIELDS.map((name, i) => [name, engine._sl_value(i)]));
  function pullScreen() {
    screen = Array.from({length: state.bins}, (_, i) => engine._sl_screen(i));
    theory = Array.from({length: state.bins}, (_, i) => engine._sl_screen_theory(i));
    ideal = Array.from({length: state.bins}, (_, i) => engine._sl_screen_ideal(i));
  }
  function paintImage() {
    const ptr = engine._sl_image();
    const pixels = new Uint8ClampedArray(engine.HEAPU8.buffer, ptr, 256 * 256 * 4);
    offscreen.getContext('2d').putImageData(new ImageData(new Uint8ClampedArray(pixels), 256, 256), 0, 0);
  }
  function canvasSize(canvas) {
    const bounds = canvas.getBoundingClientRect(), dpr = Math.min(devicePixelRatio || 1, 2);
    const w = Math.round(bounds.width * dpr), h = Math.round(bounds.height * dpr);
    if (w && h && (canvas.width !== w || canvas.height !== h)) { canvas.width = w; canvas.height = h; }
    const ctx = canvas.getContext('2d'); ctx.setTransform(dpr, 0, 0, dpr, 0, 0);
    return {ctx, w: bounds.width, h: bounds.height};
  }
  const note = (name, px) => { texts = texts.filter(row => row.text !== name).concat({text: name, px}); };

  function clearParticles() { particles = []; firing = false; }
  function start(mode) {
    engine._sl_reset(mode ?? state.mode); state = read(); pullScreen(); clearParticles();
    paused = false; accumulator = 0; ticks = 0; paintImage(); render();
  }
  function setControls() {
    engine._sl_controls(Number($('sl-lambda').value), Number($('sl-sep').value), Number($('sl-width').value));
    state = read(); pullScreen(); clearParticles(); paused = true; paintImage(); render();
  }
  for (const id of ['sl-lambda', 'sl-sep', 'sl-width']) $(id).oninput = setControls;
  $('sl-scramble').oninput = event => {
    engine._sl_scramble(Number(event.target.value)); state = read(); pullScreen(); clearParticles(); paintImage(); render();
  };
  $('sl-send').onclick = () => start();
  $('sl-both').onclick = () => start(0);
  $('sl-close-b').onclick = () => start(1);
  $('sl-close-a').onclick = () => start(2);
  $('sl-watch').onclick = () => start(3);
  $('sl-pause').onclick = () => { if (state.done || (!state.time && paused)) return; paused = !paused; accumulator = 0; render(); };
  $('sl-fire').onclick = () => { if (!state.done) return; particles = []; firing = true; render(); };

  function drawWave() {
    const {ctx, w, h} = canvasSize($('sl-wave'));
    if (!w) return;
    const axis = textPx(10); note('slit wave axis', axis);
    const left = 6, right = 6, top = axis + 14, bottom = axis + 14, size = Math.min(w - left - right, h - top - bottom);
    const ox = left + (w - left - right - size) / 2, oy = top;
    ctx.clearRect(0, 0, w, h); ctx.imageSmoothingEnabled = true;
    ctx.drawImage(offscreen, ox, oy, size, size);
    const span = state.nx * state.dx, X = x => ox + (x - state.x0) / span * size, Y = y => oy + (1 - (y - state.y0) / span) * size;
    ctx.font = `${axis}px system-ui`; ctx.fillStyle = '#c9cfe3';
    ctx.textAlign = 'left'; ctx.fillText(`|ψ|², colour = phase of ψ`, ox, oy - 6);
    ctx.textAlign = 'right'; ctx.fillText(`t = ${state.time.toFixed(1)}`, ox + size, oy - 6);
    ctx.textAlign = 'center';
    for (const x of [-20, 0, 20]) ctx.fillText(`x ${x}`, X(x), oy + size + axis + 2);
    ctx.textAlign = 'left'; ctx.fillStyle = '#ffffffcc';
    if (state.mode !== 2) ctx.fillText('A', X(state.wallX + state.wallT) + 4, Y(state.sep / 2) + axis / 3);
    if (state.mode !== 1) ctx.fillText('B', X(state.wallX + state.wallT) + 4, Y(-state.sep / 2) + axis / 3);
  }
  function drawScreen() {
    const {ctx, w, h} = canvasSize($('sl-screen'));
    if (!w) return;
    const small = textPx(9); note('slit screen labels', small);
    const left = 10, right = 12, top = small + 14, bottom = small + 14;
    const half = state.screenHalf, Y = y => top + (half - y) / (2 * half) * (h - top - bottom);
    const peak = Math.max(1e-9, ...theory, ...screen) * 1.08, X = v => left + v / peak * (w - left - right);
    ctx.clearRect(0, 0, w, h); ctx.font = `${small}px system-ui`;
    ctx.fillStyle = '#c9cfe3'; ctx.textAlign = 'left'; ctx.fillText(`screen at L = ${state.screenL}`, left, top - 6);
    ctx.strokeStyle = '#ffffff40'; ctx.lineWidth = 1; ctx.beginPath(); ctx.moveTo(left, top); ctx.lineTo(left, h - bottom); ctx.stroke();
    // The predicted bright fringes m = ±1, ±2 at L tan(asin(m λ / d)).
    ctx.setLineDash([2, 4]); ctx.strokeStyle = '#ffffff30';
    for (const m of [-2, -1, 1, 2]) {
      const s = m * state.lambda / state.sep; if (Math.abs(s) >= 0.99) continue;
      const y = state.screenL * Math.tan(Math.asin(s)); if (Math.abs(y) > half) continue;
      ctx.beginPath(); ctx.moveTo(left, Y(y)); ctx.lineTo(w - right, Y(y)); ctx.stroke();
    }
    ctx.setLineDash([]);
    const yOf = i => -half + (i + 0.5) * 2 * half / state.bins;
    if (state.measuredAt >= 0) {
      ctx.beginPath(); ctx.moveTo(left, Y(yOf(0)));
      screen.forEach((v, i) => ctx.lineTo(X(v), Y(yOf(i)))); ctx.lineTo(left, Y(yOf(state.bins - 1))); ctx.closePath();
      ctx.fillStyle = COLOURS.measured + '55'; ctx.fill(); ctx.strokeStyle = COLOURS.measured; ctx.lineWidth = 2; ctx.stroke();
    }
    for (const [curve, colour, dash] of [[theory, COLOURS.theory, [6, 5]], [ideal, COLOURS.ideal, [2, 3]]]) {
      ctx.setLineDash(dash); ctx.strokeStyle = colour; ctx.lineWidth = 1.5; ctx.beginPath();
      curve.forEach((v, i) => i ? ctx.lineTo(X(v), Y(yOf(i))) : ctx.moveTo(X(v), Y(yOf(i)))); ctx.stroke();
    }
    ctx.setLineDash([]); ctx.fillStyle = '#c9cfe3'; ctx.textAlign = 'right';
    ctx.fillText(`+${half}`, w - right, top + small); ctx.fillText(`−${half}`, w - right, h - bottom - 4);
    ctx.textAlign = 'left'; ctx.fillText('intensity →', left + 4, h - 6);
  }
  function drawParticles() {
    const {ctx, w, h} = canvasSize($('sl-particles'));
    if (!w) return;
    const small = textPx(9); note('slit particle labels', small);
    const left = 40, right = 12, top = small + 12, strip = Math.max(40, (h - top) * 0.32), base = h - small - 22;
    const half = state.screenHalf, X = y => left + (y + half) / (2 * half) * (w - left - right);
    ctx.clearRect(0, 0, w, h); ctx.font = `${small}px system-ui`;
    ctx.fillStyle = '#0d1124'; ctx.fillRect(left, top, w - left - right, strip);
    ctx.fillStyle = COLOURS.particle;
    particles.forEach((y, n) => ctx.fillRect(X(y) - 1, top + 3 + jitter(n) * (strip - 6), 2, 2));
    // Histogram of the particles against the measured intensity scaled to the same count.
    const bins = 80, width = 2 * half / bins, counts = new Array(bins).fill(0);
    for (const y of particles) { const b = Math.floor((y + half) / width); if (b >= 0 && b < bins) counts[b]++; }
    const expected = screen.map(v => v * particles.length * width);
    const peak = Math.max(1, ...counts, ...expected) * 1.1, Y = c => base - c / peak * (base - top - strip - 10);
    ctx.fillStyle = COLOURS.particle + 'aa';
    counts.forEach((c, b) => { const x0 = X(-half + b * width); ctx.fillRect(x0 + 0.5, Y(c), Math.max(1, X(-half + (b + 1) * width) - x0 - 1), base - Y(c)); });
    if (particles.length) {
      ctx.strokeStyle = COLOURS.measured; ctx.lineWidth = 2; ctx.beginPath();
      expected.forEach((c, i) => { const x = X(-half + (i + 0.5) * 2 * half / state.bins); i ? ctx.lineTo(x, Y(c)) : ctx.moveTo(x, Y(c)); }); ctx.stroke();
    }
    ctx.strokeStyle = '#ffffff40'; ctx.lineWidth = 1; ctx.beginPath(); ctx.moveTo(left, base); ctx.lineTo(w - right, base); ctx.stroke();
    ctx.fillStyle = '#aab3c9'; ctx.textAlign = 'center';
    for (const y of [-100, -50, 0, 50, 100]) if (Math.abs(y) <= half) ctx.fillText(String(y), X(y), base + small + 6);
    ctx.textAlign = 'left'; ctx.fillText(`${particles.length} particles`, left, top - 4);
    ctx.textAlign = 'right'; ctx.fillText('screen position y', w - right, h - 2);
  }
  function render() {
    $('sl-lambda').value = state.lambda; $('sl-lambda-value').textContent = state.lambda.toFixed(1);
    $('sl-sep').value = state.sep; $('sl-sep-value').textContent = state.sep.toFixed(1);
    $('sl-width').value = state.width; $('sl-width-value').textContent = state.width.toFixed(1);
    $('sl-scramble').value = state.scramble; $('sl-scramble-value').textContent = `${(state.scramble / Math.PI).toFixed(2)} π`;
    $('sl-scramble-row').hidden = state.mode !== 3;
    $('sl-mode').textContent = MODES[state.mode];
    $('sl-clock').textContent = `t = ${state.time.toFixed(1)} / ${state.tMax.toFixed(1)}`;
    const ready = state.measuredAt >= 0;
    $('sl-spacing').textContent = ready && state.spacingMeasured ? state.spacingMeasured.toFixed(1) : '—';
    $('sl-spacing-theory').textContent = state.spacing.toFixed(1);
    $('sl-vis').textContent = ready ? (state.visibility * 100).toFixed(0) : '—';
    $('sl-vis-theory').textContent = (state.visibilityTheory * 100).toFixed(0);
    $('sl-through').textContent = (state.pBeyond * 100).toFixed(1);
    $('sl-count').textContent = String(particles.length);
    $('sl-note').textContent = state.mode === 3
      ? `Each run through slit B gets a random phase from [−Φ, Φ]. The cross term is multiplied by sin Φ / Φ = ${state.coherence.toFixed(2)}: at Φ = π it is gone and the screen is the sum of the two one-slit patterns.`
      : state.mode === 0
        ? `Bright fringes every L tan(asin(λ/d)) = ${state.spacing.toFixed(1)} on the screen (λL/d = ${(state.lambda * state.screenL / state.sep).toFixed(1)}); the one-slit envelope has its first zero at sin θ = λ/a.`
        : 'One slit open: a single broad envelope and no fringes. The light below the slit leans the wave toward the dim side, which Fraunhofer leaves out.';
    $('sl-pause').textContent = paused && state.time && !state.done ? 'Continue the run' : 'Hold the run';
    $('sl-pause').disabled = state.done || (!state.time && paused);
    $('sl-fire').disabled = !state.done;
    drawWave(); drawScreen(); drawParticles();
  }
  function frame(timestamp) {
    if (!last) last = timestamp;
    const dt = Math.min((timestamp - last) / 1000, 0.25); last = timestamp;
    let changed = false;
    if (!paused && !state.done) {
      accumulator += dt;
      let done = 0;
      while (accumulator >= TICK && !state.done && done < 2) {
        engine._sl_step(STEPS_PER_TICK); accumulator -= TICK; ticks++; done++;
        if (ticks % MEASURE_EVERY === 0) engine._sl_measure();
        state = read();
      }
      if (done) {
        changed = true; paintImage(); pullScreen();
        if (state.done) { paused = true; firing = true; accumulator = 0; }
      }
    } else if (firing && particles.length < PARTICLES_MAX) {
      accumulator += dt;
      while (accumulator >= TICK && particles.length < PARTICLES_MAX) {
        for (let i = 0; i < PARTICLES_PER_TICK; i++) particles.push(engine._sl_particle(particles.length));
        accumulator -= TICK;
      }
      if (particles.length >= PARTICLES_MAX) firing = false;
      changed = true;
    }
    if (changed) render();
    requestAnimationFrame(frame);
  }
  if (engine._sl_reset(0)) throw new Error('allocation');
  state = read(); pullScreen(); paintImage();
  for (const id of ['sl-lambda', 'sl-sep', 'sl-width', 'sl-scramble', 'sl-send', 'sl-both', 'sl-close-a', 'sl-close-b', 'sl-watch']) $(id).disabled = false;
  render(); requestAnimationFrame(frame);
  document.addEventListener('visibilitychange', () => { if (document.hidden) { paused = true; accumulator = 0; } });
  new ResizeObserver(() => render()).observe($('sl-wave'));
  return {
    texts: () => texts.map(row => ({...row})),
    rerender: render,
    engine,
    get snapshot() { return {...state, paused, firing, particles: particles.length}; },
    particles: () => particles.slice(),
  };
}
