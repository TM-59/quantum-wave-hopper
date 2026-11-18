# Requirements (quantum-wave-hopper)

Taken from the current code and tests; the limits come from the README Tests section and the test programs.
The `make check` trace collects only CTest testcases (`tests/ctest/CMakeLists.txt`) named with an ID below (M61).
The mapping is a declared name, not a proof that the requirement is met.

### REQ-wave-physics
The 1D solver keeps the norm (1e-10), moves and spreads a free packet as σ√(1+(t/2σ²)²), keeps the oscillator ground state,
matches the barrier formula and simulated tunnelling within 5 %; the browser adapter evolves the same ψ and matches the
packet-averaged theory within 1 %.
Tests: `test_wave`, `test_observer`.

### REQ-resonance
Two barriers: transfer matrices against the closed forms, T + R = 1, full transmission at the resonances, the Breit–Wigner line,
packets on resonance within 1 % and decay at Γ₁ within 3 %, in the browser adapter.
Tests: `test_resonance`.

### REQ-double-slit
The 2D solver keeps the norm to 1e-12, its error falls as Δt², the screen matches Fraunhofer (first fringe within 5 %,
visibility above 0.95), one slit or watching the slit removes the fringes, 40,000 particles follow the screen.
Tests: `test_slits`.

### REQ-far-side
The story layer uses the Wave API's share bit for bit; scene order, every scene's numbers, quiet heavy scenes, both choices,
and the representative `docs/narrative/` rows byte for byte.
Tests: `test_farside`, `story_bench --check`.
