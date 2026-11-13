import createWave from './physics.mjs';
import {SCENES, UI, NAMES} from './story.mjs';
import {startResonance} from './resonance.mjs';
import {startSlits} from './slits.mjs';

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
// The story (mode 3): the C nights of src/farside.c. Field names follow enum FarsideValue in src/farside.h.
const storyNames = ['night','scene','power','hours','shots','red','failures','dial','height','particles','lastN','lastK','poolN','poolK',
  'target','tol','hint','agnes','banner','loud','heavy','ending','frost','cells','redLine','ruled','line','charge','darkWaits'];
const STORY_STEPS = 60;   // solver steps per tick while a shot's packet is shown (3,300 steps in under a second)
let story = false, fs = null, shownScene = -2, heavyLog = [], shotLog = [], animating = false;
let lang = new URLSearchParams(location.search).get('lang') === 'ja' ? 'ja' : 'en';
const t$ = key => UI[lang][key];
const readStory = () => Object.fromEntries(storyNames.map((name, i) => [name, engine._fw_value(i)]));
const read = () => Object.fromEntries(names.map((name, i) => [name, engine._qw_value(i)]));
const goalReached = () => !story && state.done && Math.abs(state.pRight - 0.5) < 0.02;

function pull() {
  if (story) fs = readStory();
  state = read();
  const n = state.bins;
  density = Array.from({length: n}, (_, i) => engine._qw_density(i));
  real = Array.from({length: n}, (_, i) => engine._qw_real(i));
}
function syncControls() {
  $('height').value = state.height; $('left').value = state.left;
  $('height-value').textContent = state.height.toFixed(2);
  $('left-value').textContent = state.left.toFixed(0);
  const locked = story ? fs.scene >= 0 || fs.night >= 5 || animating : state.mode !== 0;
  $('height').disabled = locked; $('left').disabled = locked || story;
  document.querySelectorAll('[data-height]').forEach(button => button.disabled = locked);
}
function reset(mode = 0, run = false) {
  story = mode === 3; animating = false; shownScene = -2; heavyLog = []; shotLog = [];
  document.body.dataset.mode = story ? 'story' : 'observe';
  if (story) {
    const seed = Number(new URLSearchParams(location.search).get('seed')) || Math.floor(Math.random() * 2147483647);
    engine._fw_reset(seed); engine._qw_reset(0); engine._qw_controls(0, engine._fw_value(8));
  } else engine._qw_reset(mode);
  pull(); paused = !run; accumulator = 0; samples = [{time: 0, pRight: state.pRight}];
  showScene(); render();
}
function launch() {
  launched = true;
  if (!engine) return;
  if (story) return fire();
  if (state.mode !== 0) engine._qw_reset(0); else engine._qw_launch();
  pull(); samples = [{time: 0, pRight: state.pRight}]; paused = false; accumulator = 0; render();
}
function controls(left, height) {
  if (!engine) return;
  if (story) {
    if (fs.scene >= 0 || animating) return;
    engine._fw_dial(Math.round(height / 0.05)); fs = readStory(); left = 0; height = fs.height;
  }
  engine._qw_controls(left, height);
  pull(); render();
}
function togglePause() {
  if (!engine || state.done || story) return;
  paused = !paused; accumulator = 0; render();
}
$('launch').onclick = launch;
$('pause').onclick = togglePause;
$('standard').onclick = () => reset(1, true);
$('half').onclick = () => reset(2, true);
$('story').onclick = () => reset(3);
$('seal').onclick = () => seal();
$('lang').onclick = () => { lang = lang === 'en' ? 'ja' : 'en'; shownScene = -2; if (engine) { showScene(); render(); } else applyLang(); };
$('height').oninput = event => controls(state.left, Number(event.target.value));
$('left').oninput = event => controls(Number(event.target.value), state.height);
document.querySelectorAll('[data-height]').forEach(button => {
  button.onclick = () => controls(state.left, Math.round((state.height + Number(button.dataset.height)) * 100) / 100);
});
document.addEventListener('keydown', event => {
  if (!engine || event.ctrlKey || event.metaKey || event.altKey || event.target.matches('input,textarea,select')) return;
  const key = event.key.toLowerCase();
  if (![' ', 'w', 's', 'a', 'd', 'p', 'e'].includes(key)) return;
  event.preventDefault();
  if (event.repeat && (key === ' ' || key === 'p' || key === 'e')) return;
  if (key === ' ') return launch();
  if (key === 'e') return seal();
  if (key === 'p') return togglePause();
  if (state.mode && !story) return;
  const dh = key === 'w' ? 0.05 : key === 's' ? -0.05 : 0, dl = key === 'd' ? 2 : key === 'a' ? -2 : 0;
  controls(state.left + dl, Math.round((state.height + dh) * 100) / 100);
});
const stop = () => { paused = true; accumulator = 0; if (engine) render(); };
document.addEventListener('visibilitychange', () => { if (document.hidden) stop(); });
window.addEventListener('blur', stop);

// On a phone (narrow, short or touch-first screen) every canvas label is at least 14 CSS px.
const phone = matchMedia('(max-width:720px),(max-height:500px),(pointer:coarse)');
const textPx = desktop => phone.matches ? Math.max(14, desktop) : desktop;
let canvasTexts = [], launched = false, resonance = null, slits = null;
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


// ------------------------------------------------------------------ the story: shots, seals, scenes and readouts
function fire() {
  if (!story || fs.scene >= 0 || fs.night >= 5 || animating) return;
  const result = engine._fw_fire();
  fs = readStory();
  if (result === 0) {
    shotLog.push({night: fs.night, dial: fs.dial, k: fs.lastK, n: fs.lastN});
    // The shot's packet, on the observatory's own solver at the dial's height: the wave carries the odds.
    engine._qw_reset(0); engine._qw_controls(0, fs.height); engine._qw_launch();
    animating = true; paused = false; accumulator = 0;
  }
  pull(); samples = [{time: 0, pRight: state.pRight}]; render();
}
function seal() {
  if (!story || fs.scene >= 0 || fs.night >= 5 || animating) return;
  engine._fw_seal();
  pull(); showScene(); render();
}
function applyLang() {
  document.documentElement.lang = lang;
  $('lang').textContent = t$('lang'); $('lang').lang = lang === 'en' ? 'ja' : 'en'; $('lang').setAttribute('aria-label', t$('langLabel'));
  $('story-label').textContent = t$('storyButton'); $('story-eyebrow').textContent = t$('storyEyebrow'); $('story-note').textContent = t$('storyNote');
  for (const [id, key] of [['l-battery', 'battery'], ['l-hours', 'hours'], ['l-failures', 'failures'], ['l-band', 'band'], ['l-last', 'lastShot'], ['l-pool', 'pooled']]) $(id).textContent = t$(key);
  $('u-power').textContent = t$('shots'); $('u-hours').textContent = t$('h'); $('seal-label').textContent = t$('seal');
}
function storyLines(sc) {
  if (sc.endings) return sc.endings[fs.ending === 1 ? 1 : 0].map(words => ['', words]);
  return sc.lines.filter(([, , when]) => !when || (when === 'burned') === (fs.frost === 0));
}
function showScene() {
  const box = $('story-scene'), index = story && fs ? fs.scene : -1;
  if (index === shownScene) return;
  shownScene = index;
  if (index < 0) { box.hidden = true; return; }
  const sc = SCENES[index];
  box.hidden = false;
  box.className = sc.heavy ? 'heavy' : sc.endings ? 'ending' : '';
  box.dataset.scene = sc.id;
  if (sc.heavy) heavyLog.push({id: sc.id, banner: fs.banner, loud: fs.loud, bannerShown: !$('banner').hidden});
  $('story-night').textContent = fs.night < 5 ? t$('night')(fs.night + 1, t$('nights')[fs.night]) : '';
  $('story-title').textContent = sc.title[lang];
  const text = $('story-text');
  text.replaceChildren();
  for (const [who, words] of storyLines(sc)) {
    const p = document.createElement('p');
    if (who) { const b = document.createElement('b'); b.textContent = NAMES[lang][who]; b.className = who; p.append(b, ' '); }
    else p.className = 'narration';
    p.append(words[lang]);
    text.append(p);
  }
  if (sc.endings) { const p = document.createElement('p'); p.className = 'summary'; p.textContent = t$('summary')(fs.hours, fs.failures); text.append(p); }
  if (sc.note) { const p = document.createElement('p'); p.className = 'note'; p.textContent = sc.note[lang]; text.append(p); }
  const options = $('story-options');
  options.replaceChildren();
  const labels = sc.endings ? [{en: UI.en.restart, ja: UI.ja.restart}] : sc.options;
  labels.forEach((label, option) => {
    const button = document.createElement('button');
    button.textContent = label[lang];
    if (!option) button.className = 'primary';
    button.onclick = () => answer(option);
    options.append(button);
  });
  if (sc.endings) {
    const back = document.createElement('button');
    back.textContent = t$('toObservatory'); back.onclick = () => reset(0);
    options.append(back);
  }
  $('banner').hidden = true;
  box.scrollTop = 0;
  options.firstChild.focus({preventScroll: true});
}
function answer(option) {
  if (!story || fs.scene < 0) return;
  if (SCENES[fs.scene].endings) { reset(3); return; }
  engine._fw_answer(option);
  pull(); showScene();
  engine._qw_controls(0, fs.height); pull();
  render();
}
function storyStatus() {
  if (fs.night >= 5) return SCENES[6].title[lang];
  const name = t$('night')(fs.night + 1, t$('nights')[fs.night]);
  if (animating) return `${name} · V = ${fs.height.toFixed(2)}`;
  if (fs.ruled) return `${name} · ${fs.ruled > 0 ? t$('ruledHigh') : t$('ruledLow')}`;
  return name;
}
function renderStory() {
  $('s-power').textContent = fs.power.toFixed(fs.power % 1 ? 2 : 0);
  $('story-data').firstElementChild.dataset.red = String(fs.power < fs.redLine);
  $('power-bar').style.width = `${Math.min(100, fs.power / 10 * 100)}%`;
  $('s-hours').textContent = fs.hours.toFixed(2);
  $('s-failures').textContent = String(fs.failures);
  const lo = Math.round((fs.target - fs.tol) * 100), hi = Math.round((fs.target + fs.tol) * 100);
  $('s-band').textContent = t$('bandText')(lo, hi);
  $('s-last').textContent = t$('shot')(fs.lastK, fs.lastN);
  $('s-pool').textContent = t$('pool')(fs.poolK, fs.poolN);
  const notes = [fs.agnes ? t$('agnesOn') : t$('agnesOff'), `${t$('line')}: ${fs.line ? t$('lineOn') : t$('lineOff')}`, t$('hidden')];
  if (fs.hint >= 0 && fs.night < 5) notes.unshift(t$('hint')(fs.hint * 0.05));
  $('s-notes').textContent = notes.join(' · ');
  const banner = $('banner'), kind = fs.banner;
  banner.textContent = t$('banners')[kind] || '';
  banner.dataset.kind = kind >= 3 && kind <= 4 ? 'fail' : kind >= 5 ? 'wait' : 'ok';
  // A scene on screen always hides the banner; the C nights also clear it when a heavy scene opens.
  banner.hidden = !kind || fs.scene >= 0;
  document.querySelector('.scene-label').hidden = fs.scene >= 0;
  $('launch-label').textContent = t$('fire');
  $('launch').setAttribute('aria-label', t$('fire'));
  $('seal').setAttribute('aria-label', t$('seal'));
  const busy = fs.scene >= 0 || fs.night >= 5 || animating;
  $('launch').disabled = busy;
  $('seal').disabled = busy || !fs.poolN;
}

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
  const axis = textPx(10); canvasTexts = [{text: 'axis labels', px: axis}];
  ctx.font = `${axis}px system-ui`; ctx.lineWidth = 1;
  ctx.strokeStyle = '#ffffff40'; ctx.beginPath(); ctx.moveTo(left, base); ctx.lineTo(w - right, base); ctx.stroke();
  ctx.fillStyle = '#c9cfe3'; ctx.textAlign = 'center';
  for (let x = -150; x <= 150; x += 50) { ctx.fillText(String(x), X(x), base + axis + 5); }
  ctx.textAlign = 'right'; ctx.fillText('position x (ħ = m = 1)', w - right, base + 2 * axis + 10);
  ctx.textAlign = 'left'; ctx.fillText('|ψ|²', 8, top - axis - 2); ctx.fillText(DENSITY_MAX.toFixed(2), 8, top + 4); ctx.fillText('0', 8, base + 3);
  ctx.textAlign = 'right'; ctx.fillText('V', w - 8, top - axis - 2); ctx.fillText(V_MAX.toFixed(1), w - 8, top + 4);
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
  if (story) return drawShots();
  const {ctx, w, h} = canvasSize(plot), small = textPx(9), left = 44, right = 14, top = small + 11, bottom = small + 21, tMax = 165;
  canvasTexts = canvasTexts.filter(row => row.text !== 'chart labels').concat({text: 'chart labels', px: small});
  const X = t => left + t / tMax * (w - left - right), Y = p => top + (1 - p) * (h - top - bottom);
  ctx.clearRect(0, 0, w, h); ctx.font = `${small}px system-ui`; ctx.lineWidth = 1;
  ctx.textAlign = 'right';
  for (const p of [0, 0.25, 0.5, 0.75, 1]) {
    ctx.strokeStyle = p === 0.5 ? '#e69f0080' : '#2f3866'; ctx.beginPath(); ctx.moveTo(left, Y(p)); ctx.lineTo(w - right, Y(p)); ctx.stroke();
    ctx.fillStyle = '#aab3c9'; ctx.fillText(`${p * 100}`, left - 8, Y(p) + 3);
  }
  ctx.textAlign = 'center';
  for (let t = 0; t <= tMax; t += 30) ctx.fillText(String(t), X(t), h - bottom + small + 4);
  ctx.textAlign = 'left'; ctx.fillText('P [%]', 4, top - 7); ctx.textAlign = 'right'; ctx.fillText('time t [ħ/E units]', w - right, h - 2);
  ctx.strokeStyle = '#f0e4c8'; ctx.setLineDash([6, 5]); ctx.beginPath(); ctx.moveTo(left, Y(state.theory)); ctx.lineTo(w - right, Y(state.theory)); ctx.stroke(); ctx.setLineDash([]);
  ctx.strokeStyle = '#56b4e9'; ctx.lineWidth = 2; ctx.beginPath();
  samples.forEach((s, i) => { i ? ctx.lineTo(X(s.time), Y(s.pRight)) : ctx.moveTo(X(s.time), Y(s.pRight)); }); ctx.stroke();
}
/* The story's chart: tonight's shots, each the share the counter found on the far side, against tonight's band. */
function drawShots() {
  const {ctx, w, h} = canvasSize(plot), small = textPx(9), left = 44, right = 14, top = small + 11, bottom = small + 21;
  canvasTexts = canvasTexts.filter(row => row.text !== 'chart labels').concat({text: 'chart labels', px: small});
  const night = Math.min(fs.night, 4), shots = shotLog.filter(row => row.night === night), count = Math.max(10, shots.length);
  const X = i => left + (i + 0.5) / count * (w - left - right), Y = p => top + (1 - p) * (h - top - bottom);
  ctx.clearRect(0, 0, w, h); ctx.font = `${small}px system-ui`; ctx.lineWidth = 1;
  ctx.fillStyle = '#e69f0033'; ctx.fillRect(left, Y(fs.target + fs.tol), w - left - right, Y(fs.target - fs.tol) - Y(fs.target + fs.tol));
  ctx.textAlign = 'right';
  for (const p of [0, 0.25, 0.5, 0.75, 1]) {
    ctx.strokeStyle = '#2f3866'; ctx.beginPath(); ctx.moveTo(left, Y(p)); ctx.lineTo(w - right, Y(p)); ctx.stroke();
    ctx.fillStyle = '#aab3c9'; ctx.fillText(`${p * 100}`, left - 8, Y(p) + 3);
  }
  ctx.textAlign = 'left'; ctx.fillText(lang === 'ja' ? '通過 [%]' : 'through [%]', 4, top - 7);
  ctx.textAlign = 'right'; ctx.fillText(lang === 'ja' ? '今夜の一発ごと · 帯 = 今夜の幅' : "tonight's shots · band = tonight's band", w - right, h - 2);
  shots.forEach((row, i) => {
    ctx.fillStyle = '#56b4e9'; ctx.beginPath(); ctx.arc(X(i), Y(row.k / row.n), 5, 0, 2 * Math.PI); ctx.fill();
    ctx.fillStyle = '#c9cfe3'; ctx.textAlign = 'center'; ctx.fillText((row.dial * 0.05).toFixed(2), X(i), h - bottom + small + 4);
  });
}
function render() {
  $('story-data').hidden = !story; $('seal').hidden = !story;
  if (story) renderStory();
  else {
    $('banner').hidden = true; document.querySelector('.scene-label').hidden = false;
    $('launch-label').textContent = 'Launch the packet'; $('launch').removeAttribute('aria-label'); $('launch').disabled = !engine;
  }
  applyLang();
  $('mode-name').textContent = story ? t$('mode') : modeNames[state.mode];
  $('chart-title').textContent = story ? t$('chart') : 'How much has tunnelled';
  $('plot-note').textContent = story ? t$('chartNote') : 'Time in units of ħ/E · the dashed line is the exact barrier formula averaged over the packet\'s momenta';
  $('clock').textContent = `t = ${state.time.toFixed(1)}`;
  $('tr').textContent = (state.pRight * 100).toFixed(1);
  $('re').textContent = (state.pLeft * 100).toFixed(1);
  $('th').textContent = (state.theory * 100).toFixed(1);
  $('norm').textContent = state.norm.toFixed(6);
  let label;
  if (story) label = storyStatus();
  else if (state.done) label = goalReached() ? `Split complete · ${(state.pRight * 100).toFixed(1)} % through — that is half` : `Split complete · ${(state.pRight * 100).toFixed(1)} % through`;
  else if (paused) label = state.time ? 'Paused' : 'Ready to launch';
  else if (state.meanX < state.left - 25 && state.pRight < 0.005) label = 'The packet is on its way';
  else label = 'Meeting the barrier';
  $('status').textContent = label;
  $('goal').hidden = !goalReached();
  $('pause').textContent = paused ? 'Resume' : 'Pause';
  $('pause').disabled = !engine || story || state.done || (!state.time && paused);
  $('experiment-note').textContent = notes[story ? 0 : state.mode];
  if (story) { $('tr').textContent = $('re').textContent = $('th').textContent = '—'; }
  syncControls(); drawScene(); drawPlot();
  window.qwView = {snapshot: {...state, paused, goal: goalReached(), artReady: Boolean(backdrop.naturalWidth && hopper.naturalWidth), samples: samples.length,
    story, fs: story ? {...fs} : null, animating, sceneId: story && fs.scene >= 0 ? SCENES[fs.scene].id : null,
    sceneText: $('story-scene').hidden ? '' : $('story-text').textContent, bannerShown: !$('banner').hidden,
    heavyLog: heavyLog.map(row => ({...row})), shots: shotLog.length, lang}, engine};
}
function frame(timestamp) {
  if (!last) last = timestamp;
  const dt = Math.min((timestamp - last) / 1000, 0.25); last = timestamp;
  if (engine && !paused && !state.done) {
    accumulator += dt;
    let moved = false;
    while (accumulator >= TICK && !state.done) {
      engine._qw_step(story ? STORY_STEPS : STEPS_PER_TICK); accumulator -= TICK; moved = true;
      state = read(); samples.push({time: state.time, pRight: state.pRight});
    }
    if (moved) { pull(); if (state.done) { paused = true; animating = false; } render(); }
  }
  requestAnimationFrame(frame);
}
// Read-only hook for the phone playability check: canvas text sizes in CSS px, the first goal, the next tap.
const fromScene = el => { const c = scene.getBoundingClientRect(), r = el.getBoundingClientRect(); return {x: r.left + r.width / 2 - c.left, y: r.top + r.height / 2 - c.top}; };
window.__tmGate = {
  version: 1,
  get logical() { const r = scene.getBoundingClientRect(); return {w: r.width || 1, h: r.height || 1}; },
  scene: () => !engine ? 'loading' : story && fs.scene >= 0 ? `scene:${SCENES[fs.scene].id}` : paused ? 'paused' : 'play',
  goal: () => (launched && Boolean(state) && state.time > 0) || Boolean(story && fs && fs.night > 0),
  objective: () => story ? t$('goal') : $('launch').textContent,
  texts: () => canvasTexts.map(row => ({...row})).concat(resonance ? resonance.texts() : [], slits ? slits.texts() : []),
  targets: () => [],
  player: () => null,
  hint: () => !engine || launched ? null : fromScene($('launch')),
};
phone.addEventListener('change', () => { if (engine) render(); if (resonance) resonance.rerender(); if (slits) slits.rerender(); });
for (const img of [backdrop, hopper]) img.onload = () => { if (engine) render(); };
new ResizeObserver(() => { if (engine) render(); }).observe(scene);
$('height').disabled = $('left').disabled = true;
document.querySelectorAll('[data-height]').forEach(button => button.disabled = true);
try {
  engine = await createWave();
  if (engine._qw_reset(0)) throw new Error('allocation');
  for (const id of ['launch', 'standard', 'half', 'story']) $(id).disabled = false;
  reset(); requestAnimationFrame(frame);
  resonance = startResonance(engine, textPx);
  window.rsView = resonance;
  slits = startSlits(engine, textPx);
  window.slView = slits;
} catch {
  $('status').textContent = 'Wave solver unavailable — reload to retry';
  $('experiment-note').textContent = 'The solver could not load. Serve the built files over HTTP with WebAssembly enabled.';
}
