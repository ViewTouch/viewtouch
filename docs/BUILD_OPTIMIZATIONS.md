# Build Optimizations Guide

This page documents the new CMake options and recommended workflows for build-time optimizations.

Options added to the top-level `CMakeLists.txt`:

- `VT_ENABLE_LTO` (ON/OFF): enable interprocedural optimization support.
- `VT_LTO_MODE` (off|thin|full): choose ThinLTO (default `thin`) or full LTO.
- `VT_USE_CCACHE` (ON/OFF): enable use of `ccache` if present on PATH.
- `VT_ENABLE_PGO` (ON/OFF): enable PGO scaffolding.
- `VT_PGO_PHASE` (off|generate|use): set the PGO phase.

Quick examples

1) Configure a Release build with ThinLTO and ccache (recommended):

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DVT_USE_CCACHE=ON -DVT_ENABLE_LTO=ON -DVT_LTO_MODE=thin
cmake --build build -j$(nproc)
```

2) PGO workflow (generate -> run -> use):

# Generate profile
```bash
cmake -S . -B build/pgo-generate -DCMAKE_BUILD_TYPE=Release -DVT_ENABLE_PGO=ON -DVT_PGO_PHASE=generate
cmake --build build/pgo-generate -j$(nproc)
# Run the instrumented binaries to produce profile data (exercise hotspots)
./build/pgo-generate/vt_main [args...]

# Use profile
cmake -S . -B build/pgo-use -DCMAKE_BUILD_TYPE=Release -DVT_ENABLE_PGO=ON -DVT_PGO_PHASE=use
cmake --build build/pgo-use -j$(nproc)
```

Notes and recommendations

- ThinLTO is the best tradeoff between link time and optimization for incremental builds.
- `ccache` greatly speeds up iterative development builds — ensure it is installed if enabled.
- PGO requires two phases: `generate` (build instrumented binaries, run them) and `use` (rebuild using gathered profile data).
- Performance compiler flags are applied only to `Release` and `RelWithDebInfo` builds by default.

If you want CI integration, add a job that runs a Release build with `-DVT_ENABLE_LTO=ON -DVT_LTO_MODE=thin` and records binary size and a small smoke test. For PGO, consider a two-stage CI job or manual profiling step.
