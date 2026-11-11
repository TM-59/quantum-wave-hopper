CC      ?= cc
CFLAGS  ?= -std=c11 -O2 -Wall -Wextra -Wpedantic -Werror
LDLIBS  += -lm
SRC     := src/wave.c src/svg.c src/render.c src/gif.c
EMCC    ?= emcc
PYTHON  ?= python3
PORT    ?= 4216

.PHONY: all test bench web play smoke plots clean theory-figs theory-check
all: quantum-wave-hopper

quantum-wave-hopper: $(SRC) src/main.c
	$(CC) $(CFLAGS) -o $@ $^ $(LDLIBS)

test_wave: $(SRC) tests/test_wave.c
	$(CC) $(CFLAGS) -o $@ $^ $(LDLIBS)

test_gif: src/gif.c tests/test_gif.c
	$(CC) $(CFLAGS) -o $@ $^ $(LDLIBS)

test_observer: src/wave.c src/observer.c tests/test_observer.c src/wave.h src/observer.h
	$(CC) $(CFLAGS) -o $@ $(filter %.c,$^) $(LDLIBS)

test: test_wave test_gif test_observer
	./test_wave
	./test_gif
	./test_observer

bench: test_observer
	./test_observer
	@mkdir -p build
	./test_observer --trace > build/native-trace.csv

web: dist/physics.mjs
	cp web/index.html web/style.css web/app.mjs dist/
	mkdir -p dist/art
	cp assets/art/* dist/art/
	$(PYTHON) scripts/check-size.py

dist/physics.mjs: src/wave.c src/observer.c src/wave.h src/observer.h
	mkdir -p dist
	$(EMCC) $(CFLAGS) src/wave.c src/observer.c -lm --no-entry -o $@ \
	  -sMODULARIZE=1 -sEXPORT_ES6=1 -sENVIRONMENT=web,node -sFILESYSTEM=0 -sALLOW_MEMORY_GROWTH=1 \
	  -sEXPORTED_FUNCTIONS=_qw_reset,_qw_controls,_qw_launch,_qw_step,_qw_value,_qw_density,_qw_real,_qw_half_height

play: web
	$(PYTHON) -m http.server $(PORT) --bind 127.0.0.1 --directory dist

smoke: web bench quantum-wave-hopper
	node scripts/smoke.cjs

# Regenerates the CSV and the SVG charts shown in the README.
plots: quantum-wave-hopper
	mkdir -p docs
	./quantum-wave-hopper --headless docs
	./quantum-wave-hopper --gif docs/demo.gif
	head -c6 docs/demo.gif | grep -q GIF89a

# Regenerates the plotted figures and the measured-values table of docs/theory (one command).
theory_figs: src/wave.c src/observer.c src/svg.c tests/theory_figs.c src/wave.h src/observer.h src/svg.h
	$(CC) $(CFLAGS) -o $@ $(filter %.c,$^) $(LDLIBS)

theory-figs: theory_figs
	mkdir -p docs/theory/figures
	./theory_figs docs/theory/figures

# Fails when a statement of docs/theory has no figure, a link is broken, or a referenced function or test is gone.
theory-check:
	python3 scripts/theory-check.py

clean:
	rm -f quantum-wave-hopper test_wave test_gif test_observer theory_figs
	rm -rf build dist
