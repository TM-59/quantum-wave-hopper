CC      ?= cc
CFLAGS  ?= -std=c11 -O2 -Wall -Wextra -Wpedantic -Werror
LDLIBS  += -lm
SRC     := src/wave.c src/svg.c src/render.c

.PHONY: all test plots clean
all: quantum-wave-hopper

quantum-wave-hopper: $(SRC) src/main.c
	$(CC) $(CFLAGS) -o $@ $^ $(LDLIBS)

test_wave: $(SRC) tests/test_wave.c
	$(CC) $(CFLAGS) -o $@ $^ $(LDLIBS)

test: test_wave
	./test_wave

# Regenerates the CSV and the SVG charts shown in the README.
plots: quantum-wave-hopper
	mkdir -p docs
	./quantum-wave-hopper --headless docs

clean:
	rm -f quantum-wave-hopper test_wave
