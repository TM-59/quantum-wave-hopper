# Measured values

Written by `make theory-figs` (tests/theory_figs.c) from the program's own functions. Do not edit by hand.
Units: hbar = m = 1. The last column is the relative error; where the theory column is 0 it is the absolute value.

| id | what is measured | measured | theory | error |
|---|---|---|---|---|
| `norm-drift` | largest departure of the total probability from 1 over the 3300 steps of the standard experiment (theory 0) | 1.418865e-13 | 0 | 1.42e-13 |
| `energy-drift` | largest relative change of the mean energy over the same run, barrier included (theory 0) | 1.1546319e-14 | 0 | 1.15e-14 |
| `energy-packet` | mean energy of the packet k0 = 1, sigma = 10 on the grid (continuum k0^2 / 2 + 1 / (8 sigma^2)) | 0.50082722 | 0.50125 | 8.43e-04 |
| `packet-centre` | centre of a free packet (k0 = 1.5, sigma = 2) after t = 30, started at -40 (theory -40 + k0 t) | 4.9524809 | 5 | 9.50e-03 |
| `packet-width` | its width after t = 30 (theory sigma sqrt(1 + (t / 2 sigma^2)^2)) | 7.7398195 | 7.7620873 | 2.87e-03 |
| `packet-width-0` | width of the packet as launched (theory sigma = 2) | 2 | 2 | 2.22e-16 |
| `packet-norm-0` | wave_norm right after wave_gaussian (theory 1) | 1 | 1 | 0.00e+00 |
| `packet-energy` | its mean energy (theory k0^2 / 2 + 1 / (8 sigma^2) = 1.125 + 0.03125) | 1.1556337 | 1.15625 | 5.33e-04 |
| `group-speed-k1` | speed of the packet centre at k0 = 1 on the game's grid (dx 0.1, dt 0.05) (theory of the scheme: sin(k dx) / dx / (1 + (E dt / 2)^2)) | 0.99816209 | 0.99817846 | 1.64e-05 |
| `group-speed-k1-ideal` | the same against the continuum value k0 = 1 | 0.99816209 | 1 | 1.84e-03 |
| `group-speed-k3` | speed at k0 = 3 (theory of the scheme) | 2.9186797 | 2.9188113 | 4.51e-05 |
| `group-speed-k3-ideal` | the same against the continuum value 3 | 2.9186797 | 3 | 2.71e-02 |
| `continuity` | largest difference between the rate of change of the probability beyond the barrier and the current through its edge, over 3300 steps (theory 0) | 4.7181743e-14 | 0 | 4.72e-14 |
| `current-integral` | that current added up over the run against the probability found beyond the barrier at the end | 0.35012563 | 0.35012563 | 8.57e-14 |
| `current-peak` | largest current through the edge [probability per unit time] | 0.012537088 | 0 | 1.25e-02 |
| `barrier-none` | barrier_transmission with no barrier (theory 1) | 1 | 1 | 0.00e+00 |
| `barrier-game` | plane wave k = 1 at the standard barrier V = 0.6, width 2: E / V = 0.83 (theory: the tunnelling formula) | 0.34874972 | 0.34874972 | 2.22e-16 |
| `barrier-at-top` | exactly at E = V = 0.6 (theory 1 / (1 + V w^2 / 2)) | 0.45454545 | 0.45454545 | 0.00e+00 |
| `barrier-resonance` | at the first resonance above the barrier, q w = pi (theory 1) | 1 | 1 | 0.00e+00 |
| `barrier-continuity` | jump of the formula across E = V, between E = V (1 - 1e-9) and E = V (1 + 1e-9) (theory 0) | 1.2892565e-09 | 0 | 1.29e-09 |
| `barrier-thick` | a tall, thick barrier: k = 0.5, V = 5, width 4 (theory about 16 E (V - E) / V^2 exp(-2 kappa w)) | 5.5257588e-12 | 5.5257588e-12 | 2.28e-11 |
| `tunnel-sim` | transmitted probability of the simulated packet at V = 0.6 (theory: the packet-averaged formula) | 0.35012579 | 0.35097437 | 2.42e-03 |
| `tunnel-plane` | the packet-averaged formula at V = 0.6 against the single plane wave k = 1 | 0.35097437 | 0.34874972 | 6.38e-03 |
| `tunnel-worst` | largest relative gap between simulation and packet formula over 16 heights, where T is above 0.01 | 0.0053290521 | 0 | 5.33e-03 |
| `tunnel-free` | simulated transmission with no barrier (theory 1) | 1 | 1 | 3.52e-09 |
| `cn-norm-60` | norm after 60 Crank-Nicolson steps (theory 1) | 1 | 1 | 4.22e-15 |
| `euler-norm-60` | log10 of the norm after 60 explicit Euler steps on the same grid (unstable: it grows without bound) | 90.277811 | 0 | 9.03e+01 |
| `cn-norm-big-step` | norm after 50 Crank-Nicolson steps of dt = 10, 200 times the game's (theory 1: unitary for any step) | 1 | 1 | 1.69e-12 |
| `thomas-residual` | largest residual of (1 + i dt H / 2) psi_new = (1 - i dt H / 2) psi_old over the 3900 sites after one step (theory 0) | 1.6883058e-16 | 0 | 1.69e-16 |
| `step-reverse` | largest difference from the start after one step forward and one with dt reversed (theory 0) | 1.2794688e-16 | 0 | 1.28e-16 |
| `order-time` | slope of log(error) against log(dt) between dt = 0.1 and 0.025, same grid, reference dt = 0.0016 (theory 2) | 2.0021511 | 2 | 1.08e-03 |
| `error-game-dt` | L2 distance from the reference at the game's dt = 0.05 after t = 40 (a wave function of norm 1) | 0.0016229679 | 0 | 1.62e-03 |
| `ground-energy` | mean energy of the harmonic ground state on the grid (theory 1/2) | 0.49992189 | 0.5 | 1.56e-04 |
| `ground-density` | largest relative change of the density at the centre over t = 26, two periods of the phase (theory 0) | 0.00031353766 | 0 | 3.14e-04 |
| `ground-phase` | time at which the real part first crosses zero (theory pi / (2 E) = pi) | 3.15 | 3.1415927 | 2.68e-03 |
| `wall-norm` | smallest total probability while a packet reflects off the end of the grid (theory 1: nothing leaves) | 1 | 1 | 6.66e-16 |
| `wall-return` | mean position after t = 60: started at 20 moving right at 1, wall at 49.9 (theory of a perfect mirror: 2 x 50 - 20 - 60 = 20) | 20.114439 | 20 | 5.72e-03 |
| `wall-turn` | furthest the mean position gets (the wall is at 49.9) | 45.61204 | 0 | 4.56e+01 |
| `demo-steps` | time at the end of the standard demonstration (theory 3300 x 0.05) | 165 | 165 | 0.00e+00 |
| `demo-right` | probability beyond the barrier at the end (theory: the packet formula shown on screen) | 0.35012563 | 0.35097437 | 2.42e-03 |
| `demo-sum` | largest departure of left + inside + right from 1 during the run (theory 0) | 4.6629367e-15 | 0 | 4.66e-15 |
| `demo-bins-area` | sum of the 390 displayed densities x bin width 1 (theory 1) | 1 | 1 | 1.38e-13 |
| `demo-edges` | probability in the 20 bins at each end of the grid at the end (the hard walls are not reached) | 3.7758678e-18 | 0 | 3.78e-18 |
| `demo-energy` | QW_ENERGY at the end (theory: the packet's k0^2 / 2 + 1 / (8 sigma^2)) | 0.50082722 | 0.50125 | 8.43e-04 |
| `demo-locked` | barrier height after qw_controls is called in the demonstration (theory 0.6: controls locked) | 0.6 | 0.6 | 0.00e+00 |
| `half-height` | barrier height found by qw_half_height (the packet formula there should be 0.5) | 0.5 | 0.5 | 4.44e-16 |
| `half-height-value` | that height, in units where the packet's energy is 0.5 | 0.50038234 | 0 | 5.00e-01 |
| `half-right` | probability beyond the barrier at the end of the half-through demonstration (theory 0.5) | 0.4988974 | 0.5 | 2.21e-03 |
| `manual-left` | barrier position after asking for 13.2 (theory 14: snapped to the 2-wide grid of the barrier) | 14 | 14 | 0.00e+00 |
| `manual-height` | barrier height after asking for 9 (theory 1.5: clamped) | 1.5 | 1.5 | 0.00e+00 |
| `manual-left-min` | position after asking for -500 (theory -40) | -40 | -40 | 0.00e+00 |
| `manual-height-min` | height after asking for -1 (theory 0) | 0 | 0 | 0.00e+00 |
| `manual-clamp` | time after one call asking for 5000 steps (theory 400 x 0.05: one call is clamped) | 20 | 20 | 0.00e+00 |
| `manual-launch` | time after qw_launch (theory 0: a fresh packet) | 0 | 0 | 0.00e+00 |
| `manual-real` | qw_real in the bin just right of the packet centre over the square root of qw_density there: the bin centre is 0.5 from the packet centre (theory cos(k0 x 0.5)) | 0.87765896 | 0.87758256 | 8.71e-05 |
