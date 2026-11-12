// The two-barrier experiment (M2): the C adapter src/resonance.c evolves the wave, finds the resonances and
// computes every transmission; this module only reads plain numbers and draws them.
const $ = id => document.getElementById(id);
const FIELDS = ['time','mode','energy','k0','sigma','height','width','gap','norm','pLeft','pWell','pRight','tPlane','tPacket',
  'e1','g1','e2','g2','done','tMax','viewMin','viewMax','bins','zoomMin','zoomMax','zoomSites','curveE0','curveE1','curvePoints'];
const STEPS_PER_TICK = 120, TICK = 1 / 60, P_FLOOR = 1e-4;
const COLOURS = {before: '#e69f00', well: '#009e73', beyond: '#56b4e9', barrier: '#f0e4c8', energy: '#cc79a7', level: '#e6e6e6'};

export function startResonance(engine, textPx) {
  let state, density = [], zoom = [], curve = [], curvePacket = [], samples = [], runs = [], paused = true, last = 0, accumulator = 0;
  let texts = [], recorded = false;
  const read = () => Object.fromEntries(FIELDS.map((name, i) => [name, engine._rs_value(i)]));
  function pull() {
    state = read();
    density = Array.from({length: state.bins}, (_, i) => engine._rs_density(i));
    zoom = Array.from({length: state.zoomSites}, (_, i) => engine._rs_zoom(i));
  }
  const energyOf = i => state.curveE0 + (state.curveE1 - state.curveE0) * i / (state.curvePoints - 1);
  function canvasSize(canvas) {
    const bounds = canvas.getBoundingClientRect(), dpr = Math.min(devicePixelRatio || 1, 2);
    const w = Math.round(bounds.width * dpr), h = Math.round(bounds.height * dpr);
    if (w && h && (canvas.width !== w || canvas.height !== h)) { canvas.width = w; canvas.height = h; }
    const ctx = canvas.getContext('2d'); ctx.setTransform(dpr, 0, 0, dpr, 0, 0);
    return {ctx, w: bounds.width, h: bounds.height};
  }
  const note = (name, px) => { texts = texts.filter(row => row.text !== name).concat({text: name, px}); };

  function start(mode, energy) {
    if (mode === 1) engine._rs_reset(1);
    else { engine._rs_reset(0); engine._rs_energy(energy ?? state.energy); }
    pull(); samples = [{time: 0, pWell: state.pWell, pRight: state.pRight}];
    paused = false; accumulator = 0; recorded = false; render();
  }
  function setEnergy(e) {
    engine._rs_reset(0); engine._rs_energy(e);
    pull(); samples = [{time: 0, pWell: state.pWell, pRight: state.pRight}]; paused = true; recorded = false; render();
  }
  $('rs-energy').oninput = event => setEnergy(Number(event.target.value));
  $('rs-send').onclick = () => start(0);
  $('rs-e1').onclick = () => start(0, state.e1);
  $('rs-e2').onclick = () => start(0, state.e2);
  $('rs-release').onclick = () => start(1);
  $('rs-pause').onclick = () => { if (state.done || (!state.time && paused)) return; paused = !paused; accumulator = 0; render(); };

  function drawScene() {
    const {ctx, w, h} = canvasSize($('rs-scene'));
    if (!w) return;
    const axis = textPx(10); note('scene axis', axis);
    const left = 34, right = 14, top = axis + 18, base = h - 2 * axis - 14;
    const X = x => left + (x - state.viewMin) / (state.viewMax - state.viewMin) * (w - left - right);
    const peak = Math.max(0.004, ...density), Y = d => base - d / peak * (base - top) * 0.92;
    ctx.clearRect(0, 0, w, h); ctx.font = `${axis}px system-ui`;
    ctx.strokeStyle = '#ffffff40'; ctx.lineWidth = 1; ctx.beginPath(); ctx.moveTo(left, base); ctx.lineTo(w - right, base); ctx.stroke();
    ctx.fillStyle = '#c9cfe3'; ctx.textAlign = 'center';
    for (let x = -200; x <= 200; x += 100) ctx.fillText(String(x), X(x), base + axis + 4);
    ctx.textAlign = 'right'; ctx.fillText('position x', w - right, base + 2 * axis + 8);
    ctx.textAlign = 'left'; ctx.fillText(`|ψ|², top ${peak.toFixed(3)}`, left, top - 8);
    // The two barriers (each 1 wide, drawn at least 3 px) and the three regions of the density.
    ctx.fillStyle = COLOURS.barrier + 'd0';
    for (const x0 of [0, state.width + state.gap]) ctx.fillRect(X(x0), top, Math.max(3, X(x0 + state.width) - X(x0)), base - top);
    const regions = [[state.viewMin, 0, COLOURS.before], [state.width, state.width + state.gap, COLOURS.well], [2 * state.width + state.gap, state.viewMax, COLOURS.beyond]];
    for (const [from, to, colour] of regions) {
      ctx.beginPath(); let open = false;
      density.forEach((d, i) => {
        const x = state.viewMin + i + 0.5; if (x < from || x >= to) return;
        if (!open) { ctx.moveTo(X(x), base); open = true; }
        ctx.lineTo(X(x), Y(d));
      });
      if (!open) continue;
      ctx.lineTo(X(Math.min(to, state.viewMax)), base); ctx.closePath();
      ctx.fillStyle = colour + '55'; ctx.fill(); ctx.strokeStyle = colour; ctx.lineWidth = 2; ctx.stroke();
    }
  }
  function drawZoom() {
    const {ctx, w, h} = canvasSize($('rs-zoom'));
    if (!w) return;
    const axis = textPx(10); note('zoom axis', axis);
    const left = 12, right = 40, top = axis + 18, base = h - 2 * axis - 14, vMax = 1.1;
    const X = x => left + (x - state.zoomMin) / (state.zoomMax - state.zoomMin) * (w - left - right);
    const Yv = v => base - v / vMax * (base - top);
    const peak = Math.max(1e-4, ...zoom), Yd = d => base - d / peak * (base - top) * 0.9;
    ctx.clearRect(0, 0, w, h); ctx.font = `${axis}px system-ui`;
    // The well between the walls, the walls, the levels and the packet's energy on the energy axis (right).
    ctx.fillStyle = COLOURS.well + '22'; ctx.fillRect(X(state.width), top, X(state.width + state.gap) - X(state.width), base - top);
    ctx.fillStyle = COLOURS.barrier + 'b0';
    for (const x0 of [0, state.width + state.gap]) ctx.fillRect(X(x0), Yv(state.height), X(x0 + state.width) - X(x0), base - Yv(state.height));
    ctx.setLineDash([2, 4]); ctx.strokeStyle = COLOURS.level; ctx.lineWidth = 1.2;
    for (const e of [state.e1, state.e2]) { ctx.beginPath(); ctx.moveTo(X(state.width), Yv(e)); ctx.lineTo(X(state.width + state.gap), Yv(e)); ctx.stroke(); }
    ctx.setLineDash([6, 5]); ctx.strokeStyle = COLOURS.energy; ctx.lineWidth = 1.5;
    if (state.mode === 0) { ctx.beginPath(); ctx.moveTo(left, Yv(state.energy)); ctx.lineTo(w - right, Yv(state.energy)); ctx.stroke(); }
    ctx.setLineDash([]);
    ctx.beginPath();
    zoom.forEach((d, i) => { const x = state.zoomMin + (i + 0.5) * (state.zoomMax - state.zoomMin) / state.zoomSites; i ? ctx.lineTo(X(x), Yd(d)) : ctx.moveTo(X(x), Yd(d)); });
    ctx.strokeStyle = '#ffffff'; ctx.lineWidth = 2; ctx.stroke();
    ctx.strokeStyle = '#ffffff40'; ctx.lineWidth = 1; ctx.beginPath(); ctx.moveTo(left, base); ctx.lineTo(w - right, base); ctx.stroke();
    ctx.fillStyle = '#c9cfe3'; ctx.textAlign = 'center';
    for (const x of [0, 3, 6]) ctx.fillText(String(x), X(x), base + axis + 4);
    ctx.textAlign = 'left'; ctx.fillText(`|ψ|² top ${peak.toExponential(1)}`, left, top - 8);
    ctx.textAlign = 'right'; ctx.fillStyle = '#e6e6e6';
    ctx.fillText('E₁', w - 4, Yv(state.e1) + axis / 3); ctx.fillText('E₂', w - 4, Yv(state.e2) + axis / 3);
    ctx.fillStyle = '#f0e4c8'; ctx.fillText('V', w - 4, Yv(state.height) + axis / 3);
    ctx.fillText('close-up x', w - right, base + 2 * axis + 8);
  }
  function drawCurve() {
    const {ctx, w, h} = canvasSize($('rs-curve'));
    if (!w) return;
    const small = textPx(9); note('curve labels', small);
    const left = 40, right = 12, top = 2 * small + 16, bottom = small + 22;
    const X = e => left + (e - state.curveE0) / (state.curveE1 - state.curveE0) * (w - left - right), Y = t => top + (1 - t) * (h - top - bottom);
    ctx.clearRect(0, 0, w, h); ctx.font = `${small}px system-ui`; ctx.lineWidth = 1; ctx.textAlign = 'right';
    for (const t of [0, 0.5, 1]) {
      ctx.strokeStyle = '#2f3866'; ctx.beginPath(); ctx.moveTo(left, Y(t)); ctx.lineTo(w - right, Y(t)); ctx.stroke();
      ctx.fillStyle = '#aab3c9'; ctx.fillText(String(t * 100), left - 6, Y(t) + 4);
    }
    ctx.textAlign = 'center';
    for (const e of [0.1, 0.3, 0.5, 0.7]) ctx.fillText(e.toFixed(1), X(e), h - bottom + small + 6);
    ctx.textAlign = 'left'; ctx.fillText('T [%]', 4, small + 2); ctx.textAlign = 'right'; ctx.fillText('energy E', w - right, h - 2);
    // Resonances: the band E_n ± Γ_n / 2, and a label with its width and lifetime (E1 on the first line, E2 on the second).
    [[state.e1, state.g1, 'E₁'], [state.e2, state.g2, 'E₂']].forEach(([e, g, name], i) => {
      ctx.fillStyle = '#ffffff14'; ctx.fillRect(X(e - g / 2), top, Math.max(2, X(e + g / 2) - X(e - g / 2)), h - top - bottom);
      ctx.fillStyle = '#e6e6e6'; ctx.textAlign = i ? 'right' : 'left';
      const label = `${name} ${e.toFixed(3)} · Γ ${g.toFixed(3)} · τ ${(1 / g).toFixed(0)}`;
      ctx.fillText(label, i ? w - right : Math.max(left + 20, X(e) - 10), i ? 2 * small + 8 : small + 2);
    });
    ctx.strokeStyle = '#56b4e9'; ctx.lineWidth = 2; ctx.beginPath();
    curve.forEach((t, i) => { i ? ctx.lineTo(X(energyOf(i)), Y(t)) : ctx.moveTo(X(energyOf(i)), Y(t)); }); ctx.stroke();
    ctx.strokeStyle = '#f0e4c8'; ctx.setLineDash([6, 5]); ctx.beginPath();
    curvePacket.forEach((t, i) => { i ? ctx.lineTo(X(energyOf(i)), Y(t)) : ctx.moveTo(X(energyOf(i)), Y(t)); }); ctx.stroke(); ctx.setLineDash([]);
    if (state.mode === 0) { ctx.strokeStyle = COLOURS.energy; ctx.beginPath(); ctx.moveTo(X(state.energy), top); ctx.lineTo(X(state.energy), h - bottom); ctx.stroke(); }
    ctx.fillStyle = '#e69f00';
    for (const run of runs) { ctx.beginPath(); ctx.arc(X(run.energy), Y(run.pRight), 5, 0, 2 * Math.PI); ctx.fill(); }
  }
  function drawTime() {
    const {ctx, w, h} = canvasSize($('rs-time'));
    if (!w) return;
    const small = textPx(9); note('time labels', small);
    const left = 44, right = 12, top = small + 14, bottom = small + 22, tMax = Math.max(state.tMax, 1);
    const ly = p => Math.log10(Math.max(p, P_FLOOR)), lo = Math.log10(P_FLOOR);
    const X = t => left + t / tMax * (w - left - right), Y = p => top + (ly(p) / lo) * (h - top - bottom);
    ctx.clearRect(0, 0, w, h); ctx.font = `${small}px system-ui`; ctx.lineWidth = 1; ctx.textAlign = 'right';
    for (const p of [1, 0.1, 0.01, 0.001, 0.0001]) {
      ctx.strokeStyle = '#2f3866'; ctx.beginPath(); ctx.moveTo(left, Y(p)); ctx.lineTo(w - right, Y(p)); ctx.stroke();
      ctx.fillStyle = '#aab3c9'; ctx.fillText(p >= 0.01 ? String(p * 100) : (p * 100).toFixed(p >= 0.001 ? 1 : 2), left - 6, Y(p) + 4);
    }
    ctx.textAlign = 'center';
    const stepT = tMax > 500 ? 200 : tMax > 250 ? 100 : 50;
    for (let t = 0; t <= tMax; t += stepT) ctx.fillText(String(t), X(t), h - bottom + small + 6);
    ctx.textAlign = 'left'; ctx.fillText('P [%], log scale', 4, top - 6); ctx.textAlign = 'right'; ctx.fillText('time t', w - right, h - 2);
    for (const [key, colour] of [['pWell', COLOURS.well], ['pRight', COLOURS.beyond]]) {
      ctx.strokeStyle = colour; ctx.lineWidth = 2; ctx.beginPath();
      samples.forEach((s, i) => { i ? ctx.lineTo(X(s.time), Y(s[key])) : ctx.moveTo(X(s.time), Y(s[key])); }); ctx.stroke();
    }
    // Theory: the packet's transmission (packet mode) or exp(-Γ t) from t = 30 (release mode).
    ctx.strokeStyle = '#f0e4c8'; ctx.setLineDash([6, 5]); ctx.beginPath();
    if (state.mode === 0) { ctx.moveTo(left, Y(state.tPacket)); ctx.lineTo(w - right, Y(state.tPacket)); }
    else {
      const anchor = samples.find(s => s.time >= 30);
      if (anchor) { ctx.moveTo(X(30), Y(anchor.pWell)); ctx.lineTo(X(tMax), Y(anchor.pWell * Math.exp(-state.g1 * (tMax - 30)))); }
    }
    ctx.stroke(); ctx.setLineDash([]);
  }
  function render() {
    $('rs-energy').value = state.energy; $('rs-energy-value').textContent = state.energy.toFixed(3);
    $('rs-mode').textContent = state.mode ? 'RELEASED FROM THE WELL' : 'PACKET AT ONE ENERGY';
    $('rs-clock').textContent = `t = ${state.time.toFixed(1)}`;
    $('rs-tr').textContent = (state.pRight * 100).toFixed(1);
    $('rs-well').textContent = (state.pWell * 100).toFixed(1);
    $('rs-th').textContent = state.mode ? '—' : (state.tPacket * 100).toFixed(1);
    $('rs-plane').textContent = state.mode ? '—' : (state.tPlane * 100).toFixed(1);
    $('rs-e1-label').textContent = `Resonance 1: E₁ = ${state.e1.toFixed(3)}`;
    $('rs-e2-label').textContent = `Resonance 2: E₂ = ${state.e2.toFixed(3)}`;
    $('rs-note').textContent = state.mode
      ? `The lowest standing wave of the gap leaks out through both walls. Its tail decays as exp(−Γ₁t) with Γ₁ = ${state.g1.toFixed(4)}: the lifetime ħ/Γ₁ is ${(1 / state.g1).toFixed(1)}, the width of the first resonance line.`
      : 'The packet (σ = 20) holds a spread of energies; only the part inside the resonance line passes, so even on resonance about half gets through.';
    $('rs-pause').textContent = paused && state.time && !state.done ? 'Continue the run' : 'Hold the run';
    $('rs-pause').disabled = state.done || (!state.time && paused);
    drawScene(); drawZoom(); drawCurve(); drawTime();
  }
  function frame(timestamp) {
    if (!last) last = timestamp;
    const dt = Math.min((timestamp - last) / 1000, 0.25); last = timestamp;
    if (!paused && !state.done) {
      accumulator += dt;
      let moved = false;
      while (accumulator >= TICK && !state.done) {
        engine._rs_step(STEPS_PER_TICK); accumulator -= TICK; moved = true;
        state = read(); samples.push({time: state.time, pWell: state.pWell, pRight: state.pRight});
      }
      if (moved) {
        pull();
        if (state.done) { paused = true; if (state.mode === 0 && !recorded) { runs.push({energy: state.energy, pRight: state.pRight, theory: state.tPacket}); recorded = true; } }
        render();
      }
    }
    requestAnimationFrame(frame);
  }
  if (engine._rs_reset(0)) throw new Error('allocation');
  curve = Array.from({length: engine._rs_value(28)}, (_, i) => engine._rs_curve(i));
  curvePacket = curve.map((_, i) => engine._rs_curve_packet(i));
  pull(); samples = [{time: 0, pWell: state.pWell, pRight: state.pRight}];
  for (const id of ['rs-energy', 'rs-send', 'rs-e1', 'rs-e2', 'rs-release']) $(id).disabled = false;
  // (the pause button is enabled by render while a run is going)
  render(); requestAnimationFrame(frame);
  const stop = () => { paused = true; accumulator = 0; };
  document.addEventListener('visibilitychange', () => { if (document.hidden) stop(); });
  new ResizeObserver(() => render()).observe($('rs-scene'));
  return {
    texts: () => texts.map(row => ({...row})),
    rerender: render,
    get snapshot() { return {...state, paused, runs: runs.map(r => ({...r})), samples: samples.length}; },
  };
}
