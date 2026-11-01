# M4: a dusk observatory for one wave packet

2026-11-01. Direction: flat gouache dusk landscape (warm sky, violet ridges, teal water) behind
a dark plotting band, with one small observer character. Computed curves use the
Okabe–Ito colour-blind-safe pair: orange for the reflected part, sky blue for the transmitted part,
cream for the barrier and reddish purple for the mean energy.

## Before and after

The original terminal renderer draws the density as `#` columns and the barrier as `=` on the
bottom line, with no axes, units or legend. `docs/images/before-terminal.jpg` captures the
actual `--snapshot` output, which is still available, as are the GIF, CSV and SVG outputs.

The browser view draws the density computed by the same C solver (WebAssembly), split at the
barrier, with an x axis in ħ = m = 1 units, a left |ψ|² axis, a right V axis, Re ψ, the
packet's mean energy, and live transmitted / reflected / theory / norm readouts. The painting
does not determine anything: it sits behind a darkened band and the label on the scene says so.

## Generated assets: 5 generated, 2 accepted

Generator: Stable Diffusion 1.5 through stable-diffusion.cpp on the CPU of a local machine
(no API key, no paid service), 24 steps, CFG 7. Original designs; no third-party image, sound
or font files. Every generated image was inspected by eye before deciding.

Negative prompt for all: `text, letters, watermark, logo, signature, words, frame, border,
people, human, blurry, deformed, extra limbs`.

| Candidate | Seed, size | Decision |
|---|---|---|
| bg-a | 11, 768×448 | Rejected: calm layered ridges, but the top and bottom edges show a pale border strip and the palette is flat grey-blue; the curves had less contrast over it. |
| bg-b | 23, 768×448 | **Accepted** as `assets/art/observatory.jpg`: warm dusk sky, violet ridge line, teal water, dark foreground. No text or marks. Shaved 3 px top/bottom, resized to 1024 px wide, JPEG q84 (45,360 B). Drawn as the scene background. |
| hero-a | 5, 448×448 | Rejected: character pushed to the right edge, one arm cut off, holding an unclear object. |
| hero-b | 17, 448×448 | **Accepted** as `assets/art/hopper-sprite.png` ("Hop"): a teal one-eyed observer on a small stand, coherent shapes, no text. Resized to 256×256 PNG (58,187 B). Drawn as the guide portrait and as a round marker on the highest point of the computed density. |
| hero-c | 29, 448×448 | Rejected: friendly blue robot, but the face is malformed (one large eye plus two tiny scribbled eyes). |

Background prompt: `wide gouache painting of a quiet night observatory valley, deep indigo sky,
soft glowing teal and amber ripples of light flowing across a dark calm lake, distant mountains,
empty center sky, calm, painterly texture, muted palette, original design, no logos, no text`.

Character prompt: `cute small round explorer robot character, full body, big round glowing eyes,
single antenna with a tiny light, teal and cream body, small feet, friendly, standing, centered,
plain white background, gouache children's book illustration, original design, no logos, no text`.
The accepted image does not follow the prompt literally (single eye, coloured background); it was
kept because it reads as an "observer", fits the palette and has no defects. It is drawn inside a
circle, so no transparency is needed.

## Limits

Hop is decoration. It rests on the tallest density bin, which is not a measured particle
position; the page and README say so. No human first-play evaluation, real mobile-device test
or real-GPU measurement. No publication or submission.
