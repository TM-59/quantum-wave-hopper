/* Local verification: native/WASM comparison, real controls and actual screenshots. */
const {chromium} = require('playwright');
const assert = require('node:assert/strict');
const fs = require('node:fs');
const http = require('node:http');
const path = require('node:path');
const {pathToFileURL} = require('node:url');
const {execFileSync} = require('node:child_process');
const root = path.resolve('dist');
const mime = {'.html':'text/html','.mjs':'text/javascript','.wasm':'application/wasm','.css':'text/css','.png':'image/png','.jpg':'image/jpeg'};
const server = http.createServer((req, res) => {
  const file = path.resolve(root, '.' + decodeURIComponent(new URL(req.url, 'http://localhost').pathname));
  const target = file === root ? path.join(root, 'index.html') : file;
  if (!target.startsWith(root + path.sep)) { res.writeHead(403).end(); return; }
  fs.readFile(target, (error, data) => {
    if (error) res.writeHead(404).end();
    else { res.setHeader('Content-Type', mime[path.extname(target)] || 'application/octet-stream'); res.end(data); }
  });
});
let browser, checks = 0;
const check = (value, message) => { assert.ok(value, message); checks++; };
const snapshot = page => page.evaluate(() => window.qwView.snapshot);
const capture = (page, name) => page.screenshot({path: `docs/images/${name}.jpg`, type: 'jpeg', quality: 86, fullPage: true});
async function ready(page, url) { await page.goto(url); await page.waitForFunction(() => window.qwView?.snapshot?.artReady); await page.clock.install(); }
// One run is 3,300 solver steps = 412.5 ticks of 1/60 s.
const RUN_MS = 7200;

async function compareWasm() {
  const createWave = (await import(pathToFileURL(path.resolve('dist/physics.mjs')).href)).default;
  const engine = await createWave({wasmBinary: fs.readFileSync('dist/physics.wasm')});
  const rows = fs.readFileSync('build/native-trace.csv', 'utf8').trim().split('\n').slice(1).map(line => line.split(',').map(Number));
  let row = 0, maxError = 0;
  const bins = [0, 150, 195, 200, 300];
  for (let mode = 0; mode < 3; mode++) {
    engine._qw_reset(mode);
    for (let step = 0; step <= 3300; step++) {
      if (step % 100 === 0) {
        const native = rows[row++];
        assert.equal(native[0], mode); assert.equal(native[1], step);
        const values = [...Array(18).keys()].map(f => engine._qw_value(f))
          .concat(bins.map(b => engine._qw_density(b)), [engine._qw_real(150), engine._qw_real(200)]);
        values.forEach((value, i) => {
          const error = Math.abs(value - native[i + 2]);
          maxError = Math.max(maxError, error);
          assert.ok(Number.isFinite(error) && error < 1e-9, `WASM/native mode=${mode} step=${step} column=${i}: ${error}`);
        });
      }
      if (mode === 0 && step === 300) engine._qw_controls(10, 0.7);
      if (mode === 0 && step === 600) engine._qw_controls(10, 0.4);
      engine._qw_step(1);
    }
  }
  check(row === rows.length, 'all native samples checked');
  return {rows: row, columns: rows[0].length - 2, maxError, tolerance: 1e-9};
}

async function run() {
  const started = performance.now(), equivalence = await compareWasm();
  fs.mkdirSync('docs/images', {recursive: true});
  await new Promise((resolve, reject) => { server.once('error', reject); server.listen(0, '127.0.0.1', resolve); });
  const origin = `http://127.0.0.1:${server.address().port}`;
  browser = await chromium.launch({headless: true, args: ['--no-sandbox', '--enable-unsafe-swiftshader']});
  const errors = [], external = [];
  const context = await browser.newContext({viewport: {width: 1440, height: 1050}, deviceScaleFactor: 1});
  const track = page => {
    page.on('pageerror', error => errors.push(error.message));
    page.on('console', msg => { if (msg.type() === 'error') errors.push(msg.text()); });
    page.on('request', req => { if (!req.url().startsWith(origin) && !req.url().startsWith('data:')) external.push(req.url()); });
  };
  // Before: the original terminal frame, captured as text.
  const before = await context.newPage();
  const terminal = execFileSync('./quantum-wave-hopper', ['--snapshot'], {encoding: 'utf8'});
  await before.setContent('<html lang="en"><body style="margin:0;background:#080a18;color:#dce8e9;padding:40px"><p style="font:16px monospace">Original terminal output / ./quantum-wave-hopper --snapshot</p><pre style="font:16px/1.3 monospace"></pre></body></html>');
  await before.locator('pre').evaluate((el, text) => el.textContent = text, terminal);
  await capture(before, 'before-terminal'); await before.close();

  const page = await context.newPage(); track(page); await ready(page, origin + '/');
  const initial = await snapshot(page);
  check(initial.paused && initial.time === 0 && Math.abs(initial.norm - 1) < 1e-12, 'starts paused with a normalised packet');
  await capture(page, 'observatory');
  // Keyboard reaches the C barrier, launch runs the solver, pause freezes it.
  await page.keyboard.press('w'); await page.keyboard.press('d');
  let s = await snapshot(page);
  check(Math.abs(s.height - 0.65) < 1e-12 && s.left === 2, 'keyboard moves and raises the barrier in C');
  await page.keyboard.press('s'); await page.keyboard.press('a');
  await page.keyboard.press(' '); await page.clock.runFor(2500);
  s = await snapshot(page);
  check(s.time > 10 && !s.paused, 'launch advances the solver');
  await page.keyboard.press('p'); const frozen = await snapshot(page); await page.clock.runFor(800);
  check((await snapshot(page)).time === frozen.time, 'pause freezes the C state');
  await page.keyboard.press('p'); await page.clock.runFor(3300);
  s = await snapshot(page);
  check(s.pRight > 0.05 && s.pLeft > 0.3 && !s.done, 'packet is splitting at the barrier');
  await page.keyboard.press('p'); await capture(page, 'splitting'); await page.keyboard.press('p');
  await page.clock.runFor(RUN_MS);
  const standard = await snapshot(page);
  check(standard.done && standard.paused, 'run stops at 3,300 steps');
  check(Math.abs(standard.pRight - standard.theory) < 0.01, `manual default T ${standard.pRight} vs theory ${standard.theory}`);
  check(Math.abs(standard.norm - 1) < 1e-10, 'norm conserved in the browser');
  check(!standard.goal && await page.locator('#goal').isHidden(), 'one third is not reported as half');
  await capture(page, 'split-complete');
  // Theory-based half demonstration reaches the game goal and locks the controls.
  await page.getByRole('button', {name: 'Exactly half (theory)'}).click();
  check(await page.locator('#height').isDisabled(), 'demo locks the barrier');
  await page.clock.runFor(RUN_MS + 500);
  const half = await snapshot(page);
  check(half.done && Math.abs(half.pRight - 0.5) < 0.02 && half.goal, `half demo reaches the goal (${half.pRight})`);
  check(await page.locator('#goal').isVisible(), 'goal banner shown');
  await capture(page, 'half-through');
  // Manual play towards the goal: lowering the barrier lets more through.
  await page.getByRole('button', {name: 'Launch the packet'}).click();
  await page.locator('#height').fill('0.3'); await page.clock.runFor(RUN_MS + 500);
  const low = await snapshot(page);
  check(low.mode === 0 && low.done && low.pRight > 0.8 && low.height === 0.3, `low barrier lets most through (${low.pRight})`);
  check(await page.locator('body').evaluate(el => el.scrollWidth <= innerWidth), 'desktop has no horizontal overflow');
  await page.getByRole('link', {name: 'How it works'}).click();
  check(page.url().endsWith('#field-notes'), 'field notes link');

  const mobileContext = await browser.newContext({viewport: {width: 390, height: 844}, deviceScaleFactor: 1, isMobile: true, hasTouch: true});
  const mobile = await mobileContext.newPage(); track(mobile); await ready(mobile, origin + '/');
  await mobile.getByRole('button', {name: 'Raise the barrier'}).tap();
  check(Math.abs((await snapshot(mobile)).height - 0.65) < 1e-12, 'touch reaches C');
  await mobile.getByRole('button', {name: 'Lower the barrier'}).tap();
  await mobile.getByRole('button', {name: 'Launch the packet'}).tap(); await mobile.clock.runFor(4500);
  check((await snapshot(mobile)).time > 30, 'touch launch runs the solver');
  await mobile.getByRole('button', {name: 'Pause'}).tap();
  check(await mobile.locator('body').evaluate(el => el.scrollWidth <= innerWidth), '390px has no overflow');
  for (const el of await mobile.locator('button').all()) {
    const rect = await el.boundingBox(); check(rect.width >= 44 && rect.height >= 44, 'buttons have 44px touch target');
  }
  await mobile.evaluate(() => scrollTo(0, 0)); await capture(mobile, 'mobile'); await mobileContext.close();

  // Missing art must not block the physics.
  const fallback = await context.newPage(); track(fallback);
  await fallback.route('**/art/**', route => route.fulfill({status: 404, body: ''}));
  await fallback.goto(origin + '/'); await fallback.waitForFunction(() => window.qwView?.snapshot);
  await fallback.clock.install(); await fallback.getByRole('button', {name: 'Launch the packet'}).click(); await fallback.clock.runFor(600);
  check((await snapshot(fallback)).time > 0 && !(await snapshot(fallback)).artReady, 'art failure leaves the solver usable');
  await fallback.close();
  // The 404 of the deliberately broken art is expected; only count errors from normal pages.
  const normalErrors = errors.filter(e => !/404/.test(e));
  // Solver failure is a visible, noninteractive error state.
  const broken = await context.newPage();
  await broken.route('**/physics.wasm', route => route.fulfill({status: 200, body: ''}));
  await broken.goto(origin + '/'); await broken.getByText('Wave solver unavailable — reload to retry').waitFor();
  check(await broken.locator('#launch').isDisabled() && await broken.locator('#height').isDisabled(), 'WASM failure disables controls');
  await broken.close();
  check(normalErrors.length === 0, `console/page errors ${normalErrors.join(';')}`);
  check(external.length === 0, 'no external requests');
  const result = {checks, seconds: (performance.now() - started) / 1000, equivalence, errors: normalErrors.length, externalRequests: external.length,
    standard: {pRight: standard.pRight, theory: standard.theory, norm: standard.norm, time: standard.time},
    half: {height: half.height, pRight: half.pRight, theory: half.theory}, low: {height: low.height, pRight: low.pRight},
    screenshots: fs.readdirSync('docs/images').filter(file => file.endsWith('.jpg')).sort()};
  fs.writeFileSync('docs/validation.json', JSON.stringify(result, null, 2) + '\n'); console.log(JSON.stringify(result, null, 2));
}
run().catch(error => { console.error(error); process.exitCode = 1; }).finally(async () => { if (browser) await browser.close(); server.close(); });
