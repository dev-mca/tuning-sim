# tuning-sim

Desktop simulator for learning engine tuning: ECU maps, turbo/supercharger, gearbox and dyno,
physically synthesised engine sound (an inline six must sound like an inline six) and a
cockpit-style UI with gauges, heatmap tables and dyno graphs.

> A tune that works here is not a release for a real car.

## Architecture (short)

Monorepo with a hexagonal architecture (ports & adapters). One application, two processes:

| Part | Tech | Role |
|---|---|---|
| `core/` | C++20, CMake | `simcore`: deterministic headless simulation process. It also generates and plays the **sound**. |
| `ui/` | TypeScript, React, Vite | Cockpit UI, talks to simcore via WebSocket on `127.0.0.1` (JSON, versioned) |
| `lab/` | Python | Calculation lab: plots and golden files for the C++ tests |
| `shared/` | JSON, WAV | Engine definitions, ECU tables, impulse responses, golden files |

Later, Tauri v2 packages the UI and starts simcore as a sidecar. Audio never goes through the
WebSocket.

## Build (C++)

```bash
cmake --preset debug
cmake --build --preset debug
./build/debug/core/simcore
```

## Documentation

- [`docs/plan.md`](docs/plan.md): architecture, tools, iterations, acceptance criteria
- [`docs/research.md`](docs/research.md): physics, maths, sources
- [`docs/setup.md`](docs/setup.md): toolchain and versions
- [`docs/adr/`](docs/adr/): architecture decision records
- [`THIRD_PARTY_NOTICES.md`](THIRD_PARTY_NOTICES.md): third-party licences
