import createWave from './physics.mjs';

const $ = id => document.getElementById(id);
const names = ['time','left','height','width','norm','meanX','pLeft','pInside','pRight','energy','theory','mode','k0','sigma','xMin','xMax','bins','done'];
const scene = $('scene'), plot = $('plot');
const backdrop = new Image(), hopper = new Image();
backdrop.src = 'art/observatory.jpg'; hopper.src = 'art/hopper-sprite.png';
const STEPS_PER_TICK = 8, TICK = 1 / 60, VIEW = [-150, 150], DENSITY_MAX = 0.05, V_MAX = 1.5;
const modeNames = ['MANUAL BARRIER','STANDARD BARRIER','EXACTLY HALF'];
const notes = [
  'Your barrier, your packet. Launch it and read the split on the right of the barrier.',
  'Height 0.6 sits above the packet\'s mean energy of about 0.50. Classically nothing passes; the computed share is about 35 %. Controls are locked during the experiment.',
  'The height is found by bisection on the exact barrier formula so that theory gives 50 %. The solver is not told the answer. Controls are locked during the experiment.'
];
let engine, state, paused = true, last = 0, accumulator = 0, samples = [], density = [], real = [];
const read = () => Object.fromEntries(names.map((name, i) => [name, engine._qw_value(i)]));
const goalReached = () => state.done && Math.abs(state.pRight - 0.5) < 0.02;

function pull() {
  state = read();
  const n = state.bins;
  density = Array.from({length: n}, (_, i) => engine._qw_density(i));
  real = Array.from({length: n}, (_, i) => engine._qw_real(i));
}
function syncControls() {
  $('height').value = state.height; $('left').value = state.left;
  $('height-value').textContent = state.height.toFixed(2);
  $('left-value').textContent = state.left.toFixed(0);
  const locked = state.mode !== 0;
  $('height').disabled = $('left').disabled = locked;
  document.querySelectorAll('[data-height]').forEach(button => button.disabled = locked);
}
function reset(mode = 0, run = false) {
  engine._qw_reset(mode);
  pull(); paused = !run; accumulator = 0; samples = [{time: 0, pRight: state.pRight}];
  render();
}
function launch() {
  if (!engine) return;
  if (state.mode !== 0) engine._qw_reset(0); else engine._qw_launch();
  pull(); samples = [{time: 0, pRight: state.pRight}]; paused = false; accumulator = 0; render();
}
function controls(left, height) {
  if (!engine) return;
  engine._qw_controls(left, height);
  pull(); render();
}
function togglePause() {
  if (!engine || state.done) return;
  paused = !paused; accumulator = 0; render();
}
$('launch').onclick = launch;
$('pause').onclick = togglePause;
$('standard').onclick = () => reset(1, true);
$('half').onclick = () => reset(2, true);
$('height').oninput = event => controls(state.left, Number(event.target.value));
$('left').oninput = event => controls(Number(event.target.value), state.height);
document.querySelectorAll('[data-height]').forEach(button => {
  button.onclick = () => controls(state.left, Math.round((state.height + Number(button.dataset.height)) * 100) / 100);
});
document.addEventListener('keydown', event => {
  if (!engine || event.ctrlKey || event.metaKey || event.altKey || event.target.matches('input,textarea,select')) return;
  const key = event.key.toLowerCase();
  if (![' ', 'w', 's', 'a', 'd', 'p'].includes(key)) return;
  event.preventDefault();
  if (event.repeat && (key === ' ' || key === 'p')) return;
  if (key === ' ') return launch();
  if (key === 'p') return togglePause();
  if (state.mode) return;
  const dh = key === 'w' ? 0.05 : key === 's' ? -0.05 : 0, dl = key === 'd' ? 2 : key === 'a' ? -2 : 0;
  controls(state.left + dl, Math.round((state.height + dh) * 100) / 100);
});
const stop = () => { paused = true; accumulator = 0; if (engine) render(); };
document.addEventListener('visibilitychange', () => { if (document.hidden) stop(); });
window.addEventListener('blur', stop);

function canvasSize(canvas) {
  const bounds = canvas.getBoundingClientRect(), dpr = Math.min(devicePixelRatio || 1, 2);
  const w = Math.round(bounds.width * dpr), h = Math.round(bounds.height * dpr);
  if (canvas.width !== w || canvas.height !== h) { canvas.width = w; canvas.height = h; }
  const ctx = canvas.getContext('2d'); ctx.setTransform(dpr, 0, 0, dpr, 0, 0);
  return {ctx, w: bounds.width, h: bounds.height};
}
function cover(ctx, img, w, h) {
  const scale = Math.max(w / img.naturalWidth, h / img.naturalHeight);
  ctx.drawImage(img, (w - img.naturalWidth * scale) / 2, (h - img.naturalHeight * scale) / 2, img.naturalWidth * scale, img.naturalHeight * scale);
}
const binX = i => state.xMin + i + 0.5;   // 10 grid points of 0.1 per bin

function drawScene() {
  const {ctx, w, h} = canvasSize(scene);
  ctx.clearRect(0, 0, w, h);
  if (backdrop.complete && backdrop.naturalWidth) cover(ctx, backdrop, w, h);
  else { ctx.fillStyle = '#141a36'; ctx.fillRect(0, 0, w, h); }
  // Darken the plotting band so the computed curves keep their contrast over the painting.
  const narrow = w < 560, left = narrow ? 30 : 46, right = narrow ? 30 : 46, top = h * (narrow ? 0.38 : 0.26), base = h * 0.82;
  const g = ctx.createLinearGradient(0, top - 30, 0, h);
  g.addColorStop(0, '#0b0f2200'); g.addColorStop(0.25, '#0b0f22b8'); g.addColorStop(1, '#0b0f22e6');
  ctx.fillStyle = g; ctx.fillRect(0, top - 30, w, h - top + 30);
  const X = x => left + (x - VIEW[0]) / (VIEW[1] - VIEW[0]) * (w - left - right);
  const Yd = d => base - Math.min(d / DENSITY_MAX, 1) * (base - top);
  const Yv = v => base - Math.min(v / V_MAX, 1) * (base - top);
  // Axes and units.
  ctx.font = '10px system-ui'; ctx.lineWidth = 1;
  ctx.strokeStyle = '#ffffff40'; ctx.beginPath(); ctx.moveTo(left, base); ctx.lineTo(w - right, base); ctx.stroke();
  ctx.fillStyle = '#c9cfe3'; ctx.textAlign = 'center';
  for (let x = -150; x <= 150; x += 50) { ctx.fillText(String(x), X(x), base + 15); }
  ctx.textAlign = 'right'; ctx.fillText('position x (ħ = m = 1)', w - right, base + 30);
  ctx.textAlign = 'left'; ctx.fillText('|ψ|²', 8, top - 6); ctx.fillText(DENSITY_MAX.toFixed(2), 8, top + 4); ctx.fillText('0', 8, base + 3);
  ctx.textAlign = 'right'; ctx.fillText('V', w - 8, top - 6); ctx.fillText(V_MAX.toFixed(1), w - 8, top + 4);
  // Barrier and the packet's mean energy on the potential axis.
  const bx0 = X(state.left), bx1 = X(state.left + state.width);
  ctx.fillStyle = '#f0e4c8d0'; ctx.fillRect(bx0, Yv(state.height), Math.max(bx1 - bx0, 3), base - Yv(state.height));
  ctx.strokeStyle = '#cc79a7'; ctx.setLineDash([6, 5]); ctx.lineWidth = 1.5;
  ctx.beginPath(); ctx.moveTo(left, Yv(state.energy)); ctx.lineTo(w - right, Yv(state.energy)); ctx.stroke(); ctx.setLineDash([]);
  ctx.fillStyle = '#e6b3d0'; ctx.textAlign = 'left'; ctx.fillText(`⟨E⟩ = ${state.energy.toFixed(3)}`, left + 4, Yv(state.energy) - 5);
  ctx.fillStyle = '#f0e4c8'; ctx.textAlign = 'center'; ctx.fillText(`V = ${state.height.toFixed(2)}`, (bx0 + bx1) / 2, Yv(state.height) - 6);
  // Density, split into the reflected (left of barrier) and transmitted (right) parts.
  const split = state.left + state.width;
  for (const [side, colour] of [[-1, '#e69f00'], [1, '#56b4e9']]) {
    ctx.beginPath(); let open = false;
    density.forEach((d, i) => {
      const x = binX(i); if (x < VIEW[0] || x > VIEW[1] || (side < 0 ? x >= split : x < state.left)) return;
      if (!open) { ctx.moveTo(X(x), base); open = true; }
      ctx.lineTo(X(x), Yd(d));
    });
    if (!open) continue;
    const lastX = side < 0 ? Math.min(split, VIEW[1]) : VIEW[1];
    ctx.lineTo(X(lastX), base); ctx.closePath();
    ctx.fillStyle = colour + '55'; ctx.fill(); ctx.strokeStyle = colour; ctx.lineWidth = 2; ctx.stroke();
  }
  ctx.strokeStyle = '#c8cde0aa'; ctx.lineWidth = 1; ctx.setLineDash([2, 3]); ctx.beginPath();
  const amp = (base - top) * 0.45;
  real.forEach((r, i) => { const x = binX(i); if (x < VIEW[0] || x > VIEW[1]) return; const y = base - r / 0.2 * amp * 0.5; i && x > VIEW[0] + 1 ? ctx.lineTo(X(x), y) : ctx.moveTo(X(x), y); });
  ctx.stroke(); ctx.setLineDash([]);
  // Hop rests on the tallest point of the density: an illustration, not a particle position.
  let peak = 0; density.forEach((d, i) => { if (d > density[peak]) peak = i; });
  const px = Math.max(left + 20, Math.min(w - right - 20, X(binX(peak)))), py = Yd(density[peak]);
  const size = Math.max(54, Math.min(96, w * 0.075));
  if (hopper.complete && hopper.naturalWidth) {
    ctx.save(); ctx.beginPath(); ctx.arc(px, py - size / 2 - 4, size / 2, 0, 2 * Math.PI); ctx.closePath();
    ctx.fillStyle = '#f4efe2'; ctx.fill(); ctx.clip();
    ctx.drawImage(hopper, px - size / 2, py - size - 4, size, size); ctx.restore();
    ctx.strokeStyle = '#ffffffaa'; ctx.lineWidth = 2; ctx.beginPath(); ctx.arc(px, py - size / 2 - 4, size / 2, 0, 2 * Math.PI); ctx.stroke();
  } else {
    ctx.fillStyle = '#f4efe2'; ctx.beginPath(); ctx.arc(px, py - 8, 5, 0, 2 * Math.PI); ctx.fill();
  }
}
function drawPlot() {
  const {ctx, w, h} = canvasSize(plot), left = 44, right = 14, top = 20, bottom = 30, tMax = 165;
  const X = t => left + t / tMax * (w - left - right), Y = p => top + (1 - p) * (h - top - bottom);
  ctx.clearRect(0, 0, w, h); ctx.font = '9px system-ui'; ctx.lineWidth = 1;
  ctx.textAlign = 'right';
  for (const p of [0, 0.25, 0.5, 0.75, 1]) {
    ctx.strokeStyle = p === 0.5 ? '#e69f0080' : '#2f3866'; ctx.beginPath(); ctx.moveTo(left, Y(p)); ctx.lineTo(w - right, Y(p)); ctx.stroke();
    ctx.fillStyle = '#aab3c9'; ctx.fillText(`${p * 100}`, left - 8, Y(p) + 3);
  }
  ctx.textAlign = 'center';
  for (let t = 0; t <= tMax; t += 30) ctx.fillText(String(t), X(t), h - bottom + 14);
  ctx.textAlign = 'left'; ctx.fillText('P [%]', 4, top - 7); ctx.textAlign = 'right'; ctx.fillText('time t [ħ/E units]', w - right, h - 2);
  ctx.strokeStyle = '#f0e4c8'; ctx.setLineDash([6, 5]); ctx.beginPath(); ctx.moveTo(left, Y(state.theory)); ctx.lineTo(w - right, Y(state.theory)); ctx.stroke(); ctx.setLineDash([]);
  ctx.strokeStyle = '#56b4e9'; ctx.lineWidth = 2; ctx.beginPath();
  samples.forEach((s, i) => { i ? ctx.lineTo(X(s.time), Y(s.pRight)) : ctx.moveTo(X(s.time), Y(s.pRight)); }); ctx.stroke();
}
function render() {
  $('mode-name').textContent = modeNames[state.mode];
  $('clock').textContent = `t = ${state.time.toFixed(1)}`;
  $('tr').textContent = (state.pRight * 100).toFixed(1);
  $('re').textContent = (state.pLeft * 100).toFixed(1);
  $('th').textContent = (state.theory * 100).toFixed(1);
  $('norm').textContent = state.norm.toFixed(6);
  let label;
  if (state.done) label = goalReached() ? `Split complete · ${(state.pRight * 100).toFixed(1)} % through — that is half` : `Split complete · ${(state.pRight * 100).toFixed(1)} % through`;
  else if (paused) label = state.time ? 'Paused' : 'Ready to launch';
  else if (state.meanX < state.left - 25 && state.pRight < 0.005) label = 'The packet is on its way';
  else label = 'Meeting the barrier';
  $('status').textContent = label;
  $('goal').hidden = !goalReached();
  $('pause').textContent = paused ? 'Resume' : 'Pause';
  $('pause').disabled = !engine || state.done || (!state.time && paused);
  $('experiment-note').textContent = notes[state.mode];
  syncControls(); drawScene(); drawPlot();
  window.qwView = {snapshot: {...state, paused, goal: goalReached(), artReady: Boolean(backdrop.naturalWidth && hopper.naturalWidth), samples: samples.length}, engine};
}
function frame(timestamp) {
  if (!last) last = timestamp;
  const dt = Math.min((timestamp - last) / 1000, 0.25); last = timestamp;
  if (engine && !paused && !state.done) {
    accumulator += dt;
    let moved = false;
    while (accumulator >= TICK && !state.done) {
      engine._qw_step(STEPS_PER_TICK); accumulator -= TICK; moved = true;
      state = read(); samples.push({time: state.time, pRight: state.pRight});
    }
    if (moved) { pull(); if (state.done) paused = true; render(); }
  }
  requestAnimationFrame(frame);
}
for (const img of [backdrop, hopper]) img.onload = () => { if (engine) render(); };
new ResizeObserver(() => { if (engine) render(); }).observe(scene);
$('height').disabled = $('left').disabled = true;
document.querySelectorAll('[data-height]').forEach(button => button.disabled = true);
try {
  engine = await createWave();
  if (engine._qw_reset(0)) throw new Error('allocation');
  for (const id of ['launch', 'standard', 'half']) $(id).disabled = false;
  reset(); requestAnimationFrame(frame);
} catch {
  $('status').textContent = 'Wave solver unavailable — reload to retry';
  $('experiment-note').textContent = 'The solver could not load. Serve the built files over HTTP with WebAssembly enabled.';
}
