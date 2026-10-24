# Per-instance latch timing check

Run from the repository root with a C++17 compiler and the project's Boost headers.
This check only needs `timing_graph.cpp`; it does not require GLPK, LEDA or xtensor.
Keep assertions enabled (do not pass `-DNDEBUG`).

```bash
g++ -std=c++17 -Wall -Wextra -Iinclude -I"$CONDA_PREFIX/include" \
    tests/test_read_latch_timing.cpp src/timing_graph.cpp \
    -o test_read_latch_timing
./test_read_latch_timing examples/tiny_sky130/tiny.blif.out \
    examples/tiny_sky130/latch_timing.json tmp/latch_timing_checks
```

Checks all six parameters for L1/L2/L3, zero IO timing, MCR edge coefficients,
reordered input rows, and rejection of missing/duplicate/unknown instances,
missing parameters, wrong schema/units, nonfinite or unordered propagation
delays, and a missing JSON file. Failed loads must preserve previous timing.
Scratch fixtures are written only to the supplied scratch directory.

Also checks clock-edge coefficients at duty cycles 0.5, 0.4, 0.25 and 0.01,
hand-constructed schedule residuals, and rejection of duty cycles outside
the finite interval (0, 0.5]. The default remains 0.5.

For an independent LP comparison against Howard (requires GLPK, but no LEDA or xtensor):

```bash
g++ -std=c++17 -Wall -Wextra -Iinclude -I"$CONDA_PREFIX/include" \
    tests/test_mcr_duty_cycle.cpp src/timing_graph.cpp \
    src/howard.cpp src/LP.cpp -L"$CONDA_PREFIX/lib" -lglpk \
    -o test_mcr_duty_cycle
./test_mcr_duty_cycle examples/tiny_sky130/tiny.blif.out \
    examples/tiny_sky130/latch_timing.json
```

The kernel test explicitly removes v3 IO metadata before loading. It checks the 0.4620274367 ns pulse-limited
tiny baseline at 50%, solver agreement and complete schedule feasibility at all
four duty cycles, an artificial low-pulse limit, invalid LP input and overflow.
Removing pulse checks explicitly recovers the old 0.413084 ns model baseline.

The reader test derives a legacy v2 fixture from v3 input before testing the
old merged-IO contract. It also checks high/low pulse bounds, rejection of incomplete v2
data and invalid widths, failure atomicity, and legacy-v1 loading without
silently retaining pulse constraints from a previous v2 load. Regenerate the
example JSON with the converter before running these tests.

## Real IO model

The following graph/Howard check needs Boost and C++17, without GLPK/LEDA/xtensor:

```bash
g++ -std=c++17 -Wall -Wextra -Iinclude -I"$CONDA_PREFIX/include" \
    tests/test_real_io.cpp src/timing_graph.cpp src/howard.cpp \
    -o test_real_io
./test_real_io examples/tiny_sky130/tiny.blif.out \
    examples/tiny_sky130/latch_timing.json tmp/real_io_checks
```

It checks separate ports, reference isolation, the hand-derived 3.541155 ns
full-path budget, the original 10 ns IO reference times, changed budgets and
uncertainty, negative output hold delay, a below-bound infeasible period,
feedback rejection and invalid-load atomicity.

For an independent GLPK/Howard comparison on v3 data:

```bash
g++ -std=c++17 -Wall -Wextra -Iinclude -I"$CONDA_PREFIX/include" \
    tests/test_real_io_lp.cpp src/timing_graph.cpp src/howard.cpp \
    src/LP.cpp -L"$CONDA_PREFIX/lib" -lglpk -o test_real_io_lp
./test_real_io_lp examples/tiny_sky130/tiny.blif.out \
    examples/tiny_sky130/latch_timing.json
```

Keep assertions enabled for all checks. Python converter tests also cover
unsupported/missing SDC entries and discrepancies between SDC and reports.

## Howard SDC export

Build with the same Boost/GLPK dependencies, keeping assertions enabled:

```bash
g++ -std=c++17 -Wall -Wextra -Iinclude -I"$CONDA_PREFIX/include" \
    tests/test_sdc_export.cpp src/sdc_export.cpp src/timing_graph.cpp \
    src/howard.cpp src/LP.cpp -L"$CONDA_PREFIX/lib" -lglpk -o test_sdc_export
./test_sdc_export examples/tiny_sky130/tiny.blif.out \
    examples/tiny_sky130/latch_timing.json tmp/sdc_export_checks
tclsh tests/test_sdc_export.tcl tmp/sdc_export_checks/candidate_howard.sdc \
    examples/tiny_sky130/run_sta.tcl tmp/mock_sta50
tclsh tests/test_sdc_export.tcl tmp/sdc_export_checks/candidate_duty40.sdc \
    examples/tiny_sky130/run_sta.tcl tmp/mock_sta40
```

The C++ test solves Howard and GLPK at 50%/40%, exports their real candidate,
and rejects failed/mismatched results, corrupt schedules, missing conditions,
unsafe identifiers and overwriting an existing output. The real-IO reader test
also rejects wrong capacitance units and conditions inconsistent with pulse characterization.
Tcl checks execute the emitted SDC and runner with mocked STA commands, verify
waveform/latency/IO/operating conditions, 27 report destinations, missing pins
and protection of baseline/existing reports. Mock report files are explicitly
marked as such and must never be treated as timing evidence. Supply new mock
report directory names for repeated runs. These tests do not run OpenSTA or
validate physical clock implementation.
