CC      ?= cc
CFLAGS  ?= -std=c11 -O2 -Wall -Wextra -Wpedantic -Werror
LDLIBS  += -lm
SRC     := src/wave.c src/svg.c src/render.c src/gif.c
EMCC    ?= emcc
PYTHON  ?= python3
PORT    ?= 4216

.PHONY: all test bench story story-check web play smoke plots clean theory-figs theory-check
all: quantum-wave-hopper

quantum-wave-hopper: $(SRC) src/main.c
	$(CC) $(CFLAGS) -o $@ $^ $(LDLIBS)

test_wave: $(SRC) tests/test_wave.c
	$(CC) $(CFLAGS) -o $@ $^ $(LDLIBS)

test_gif: src/gif.c tests/test_gif.c
	$(CC) $(CFLAGS) -o $@ $^ $(LDLIBS)

test_observer: src/wave.c src/observer.c tests/test_observer.c src/wave.h src/observer.h
	$(CC) $(CFLAGS) -o $@ $(filter %.c,$^) $(LDLIBS)

FARSIDE   := src/wave.c src/observer.c src/farside.c
FARSIDE_H := src/wave.h src/observer.h src/farside.h tests/farside_policy.h tests/farside_run.h

test_resonance: src/wave.c src/resonance.c tests/test_resonance.c src/wave.h src/resonance.h
	$(CC) $(CFLAGS) -o $@ $(filter %.c,$^) $(LDLIBS)

test_farside: $(FARSIDE) tests/test_farside.c $(FARSIDE_H)
	$(CC) $(CFLAGS) -o $@ $(FARSIDE) tests/test_farside.c $(LDLIBS)

story_bench: $(FARSIDE) tests/story_bench.c $(FARSIDE_H)
	$(CC) $(CFLAGS) -o $@ $(FARSIDE) tests/story_bench.c $(LDLIBS)

# The story rows are compared byte for byte; the CI runs them with the Debian 13 toolchain of the Dockerfile.
test: test_wave test_gif test_observer test_resonance test_farside story_bench
	./test_wave
	./test_gif
	./test_observer
	./test_resonance
	./test_farside
	./story_bench --check docs/narrative

# Rewrites docs/narrative/{play-order,ablation,tension}.json.
story: story_bench
	mkdir -p docs/narrative
	./story_bench docs/narrative

# Regenerates every row into a scratch directory and compares the three files byte for byte.
story-check: story_bench
	rm -rf .story-check && mkdir .story-check
	./story_bench .story-check
	cmp .story-check/play-order.json docs/narrative/play-order.json
	cmp .story-check/ablation.json docs/narrative/ablation.json
	cmp .story-check/tension.json docs/narrative/tension.json
	rm -rf .story-check

bench: test_observer test_resonance
	./test_observer
	@mkdir -p build
	./test_observer --trace > build/native-trace.csv
	./test_resonance --trace > build/resonance-trace.csv

web: dist/physics.mjs
	cp web/index.html web/style.css web/app.mjs web/story.mjs web/resonance.mjs dist/
	mkdir -p dist/art
	cp assets/art/* dist/art/
	$(PYTHON) scripts/check-size.py

dist/physics.mjs: src/wave.c src/observer.c src/wave.h src/observer.h src/farside.c src/farside.h src/farside_web.c src/resonance.c src/resonance.h
	mkdir -p dist
	$(EMCC) $(CFLAGS) src/wave.c src/observer.c src/farside.c src/farside_web.c src/resonance.c -lm --no-entry -o $@ \
	  -sMODULARIZE=1 -sEXPORT_ES6=1 -sENVIRONMENT=web,node -sFILESYSTEM=0 -sALLOW_MEMORY_GROWTH=1 \
	  -sEXPORTED_FUNCTIONS=_qw_reset,_qw_controls,_qw_launch,_qw_step,_qw_value,_qw_density,_qw_real,_qw_half_height,_fw_reset,_fw_dial,_fw_fire,_fw_seal,_fw_answer,_fw_value,_fw_share,_rs_reset,_rs_energy,_rs_launch,_rs_step,_rs_value,_rs_density,_rs_zoom,_rs_curve,_rs_curve_packet

play: web
	$(PYTHON) -m http.server $(PORT) --bind 127.0.0.1 --directory dist

smoke: web bench quantum-wave-hopper
	node scripts/smoke.cjs

# Regenerates the CSV and the SVG charts shown in the README.
plots: quantum-wave-hopper
	mkdir -p docs
	./quantum-wave-hopper --headless docs
	./quantum-wave-hopper --gif docs/demo.gif
	./quantum-wave-hopper --resonance docs
	head -c6 docs/demo.gif | grep -q GIF89a

# Regenerates the plotted figures and the measured-values table of docs/theory (one command).
theory_figs: src/wave.c src/observer.c src/resonance.c src/svg.c tests/theory_figs.c src/wave.h src/observer.h src/resonance.h src/svg.h
	$(CC) $(CFLAGS) -o $@ $(filter %.c,$^) $(LDLIBS)

theory-figs: theory_figs
	mkdir -p docs/theory/figures
	./theory_figs docs/theory/figures

# Fails when a statement of docs/theory has no figure, a link is broken, or a referenced function or test is gone.
theory-check:
	python3 scripts/theory-check.py

clean:
	rm -f quantum-wave-hopper test_wave test_gif test_observer test_resonance test_farside story_bench theory_figs
	rm -rf build dist
