/* Local-only browser check of the music (M63 #29, parent TM-59/project-context#598). Not in CI.
 *   make web && node scripts/bgm-smoke.cjs   (port BGM_PORT, default 22093; docs/bgm/smoke.json)
 * On the built app it matches screen -> track -> sound at the output (RMS):
 *  1) nothing is fetched before the first press; the standard barrier plays the ballad (lab)
 *  2) The Far Side opens at dusk: story music at 0.7x; answering it starts night 1 (night)
 *  3) mute (M) during the crossfade silences the fading track too (<= -100 dBFS after 0.8 s,
 *     TM-59/project-context#668); the ♪ button is 44 px and mutes and restores
 *  4) the cue of the heavy scenes, dawn and the experiments (web/bgm.mjs in the page)
 *  5) the size of dist (10 MB)
 */
/** @type {typeof import('playwright')} */
const playwright = require(process.env.PLAYWRIGHT_MODULE || 'playwright');
const { chromium } = playwright;
const assert = require('node:assert/strict');
const fs = require('node:fs');
const http = require('node:http');
const path = require('node:path');

const root = path.resolve('dist');
const port = Number(process.env.BGM_PORT || 22093);
const server = http.createServer((req, res) => {
  const file = path.resolve(
    root,
    `.${decodeURIComponent(new URL(req.url ?? '/', 'http://localhost').pathname)}`,
  );
  const target = file === root ? path.join(root, 'index.html') : file;
  if (!target.startsWith(root + path.sep)) {
    res.writeHead(403).end();
    return;
  }
  fs.readFile(target, (error, data) => {
    if (error) res.writeHead(404).end();
    else {
      const ext = path.extname(target);
      if (ext === '.js' || ext === '.mjs') res.setHeader('Content-Type', 'text/javascript');
      if (ext === '.wasm') res.setHeader('Content-Type', 'application/wasm');
      if (ext === '.html') res.setHeader('Content-Type', 'text/html');
      res.end(data);
    }
  });
});
/** @param {string} dir @returns {number} */
const size = (dir) =>
  fs.readdirSync(dir).reduce((n, f) => {
    const p = path.join(dir, f);
    const s = fs.statSync(p);
    return n + (s.isDirectory() ? size(p) : s.size);
  }, 0);

/** @type {{ checks: object[], ok?: boolean, error?: string, seconds?: number, when?: string, fetched?: string[] }} */
const out = { checks: [] };
const t0 = Date.now();

async function main() {
  await new Promise((resolve) => server.listen(port, '127.0.0.1', () => resolve(undefined)));
  const browser = await chromium.launch({
    headless: true,
    args: ['--no-sandbox', '--autoplay-policy=user-gesture-required'],
  });
  try {
    /** @type {string[]} */
    const errors = [];
    /** @type {string[]} */
    const fetched = [];
    const page = await browser.newPage({ viewport: { width: 1280, height: 800 } });
    page.on('pageerror', (e) => errors.push(String(e)));
    page.on('request', (r) => {
      if (r.url().includes('/bgm/')) fetched.push(r.url().split('/').pop() ?? '');
    });
    await page.goto(`http://127.0.0.1:${port}/`);
    await page.waitForFunction(() => window.qwView?.snapshot && window.musicView);
    const music = async () => {
      const m = await page.evaluate(() => window.musicView);
      if (!m) throw Error('no music view');
      return m;
    };
    /** @param {string} track */
    const audible = async (track) => {
      await page.waitForFunction(
        (t) => {
          const m = window.musicView;
          return m?.playing === t && m.loaded.includes(t);
        },
        track,
        { timeout: 60000, polling: 100 },
      );
      await page.waitForTimeout(2800); // the crossfade (2.5 s) is over
      let s = await music();
      for (let i = 0; i < 10; i++) {
        await page.waitForTimeout(250);
        const n = await music();
        if (n.level > s.level) s = n;
      }
      out.checks.push({
        what: `${track} plays`,
        cue: s.cue,
        level: +s.level.toFixed(1),
        gain: s.gain,
      });
      assert(s.running, 'AudioContext running');
      assert(s.level > -45, `${track}: ${s.level.toFixed(1)} dBFS`);
      return s;
    };

    // 1) the standard barrier -> lab; nothing before the first press
    await page.waitForTimeout(1000);
    assert.equal(fetched.length, 0, 'no music is fetched before the first press');
    out.checks.push({ what: 'nothing fetched before the first press', fetched: fetched.length });
    await page.click('#standard');
    await audible('lab');

    // 2) The Far Side opens at dusk: story music at 0.7x; answering it starts night 1 (night)
    await page.click('#story');
    await page.waitForFunction(() => window.qwView?.snapshot?.sceneId === 'dusk');
    const s0 = await audible('story');
    assert.equal(s0.cue.gain, 0.7);
    await page.click('#story-options button');
    await page.waitForFunction(() => window.qwView?.snapshot?.sceneId === null);
    await page.waitForFunction(() => window.musicView?.playing === 'night', null, {
      timeout: 60000,
      polling: 50,
    });

    // 3) mute (M) in the middle of the crossfade: the fading story track goes silent too
    await page.keyboard.press('m');
    await page.waitForTimeout(800);
    const xm = await music();
    out.checks.push({
      what: 'mute during a crossfade silences the fading track too',
      level: +xm.level.toFixed(1),
    });
    assert(xm.muted && xm.level <= -100, `mute during crossfade: ${xm.level.toFixed(1)} dBFS`);
    await page.keyboard.press('m');
    const v0 = await audible('night');
    assert.equal(v0.cue.gain, 1, 'a night plays at full level (the story has no pause)');
    const box = await page.locator('#mute').boundingBox();
    assert(box && box.width >= 44 && box.height >= 44, `♪ is 44 px: ${JSON.stringify(box)}`);
    await page.click('#mute');
    await page.waitForTimeout(800);
    const bm = await music();
    assert(bm.muted && bm.level <= -100, `♪ mutes: ${bm.level.toFixed(1)}`);
    await page.click('#mute');
    let bu = await music();
    for (let i = 0; i < 16 && bu.level <= -60; i++) {
      await page.waitForTimeout(250);
      bu = await music();
    }
    assert(!bu.muted && bu.level > -60, 'the ♪ button restores the music');
    out.checks.push({
      what: '♪ mutes and restores',
      size: box,
      level: [+bm.level.toFixed(1), +bu.level.toFixed(1)],
    });

    // 4) the cues of the scenes the check does not reach (web/bgm.mjs as the page loads it)
    const cues = await page.evaluate(async () => {
      const name = './bgm.mjs';
      const { cue } = await import(name);
      const night = { story: true, heavy: false, ending: false, scene: null, over: false };
      return {
        heavy: ['letter', 'fall', 'dark'].map((scene) => cue({ ...night, scene, heavy: true })),
        dawn: cue({ ...night, scene: 'dawn', ending: true }),
        over: cue({ ...night, over: true }),
        lab: cue({ ...night, story: false }),
      };
    });
    for (const c of cues.heavy) assert.deepEqual(c, { track: 'story', gain: 0.4 });
    assert.deepEqual(cues.dawn, { track: 'dawn', gain: 1 });
    assert.deepEqual(cues.over, { track: 'dawn', gain: 1 });
    assert.deepEqual(cues.lab, { track: 'lab', gain: 1 });
    out.checks.push({
      what: 'cues: heavy scenes story 0.4x, dawn and after it dawn, the experiments lab',
    });

    // 5) dist
    const bytes = size('dist');
    out.checks.push({ what: 'dist bytes', bytes });
    assert(bytes <= 10_000_000, `dist ${bytes} > 10 MB`);
    out.fetched = [...new Set(fetched)];
    assert.deepEqual(errors, [], 'no page errors');
    out.ok = true;
    console.log('bgm-smoke OK', JSON.stringify(out.checks));
  } finally {
    await browser.close();
  }
}

main()
  .catch((e) => {
    out.ok = false;
    out.error = String(e instanceof Error ? e.stack : e);
    console.error(e);
    process.exitCode = 1;
  })
  .finally(() => {
    out.seconds = +((Date.now() - t0) / 1000).toFixed(1);
    out.when = new Date().toISOString();
    fs.writeFileSync('docs/bgm/smoke.json', `${JSON.stringify(out, null, 1)}\n`);
    server.close();
  });
