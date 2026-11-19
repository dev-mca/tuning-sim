# tuning-sim UI

Cockpit UI for tuning-sim: TypeScript (strict), React, Vite, Tailwind CSS v4.
Talks to `simcore` via WebSocket on `127.0.0.1` (from iteration 6; see `../docs/plan.md`).

```bash
cd ui
npm ci               # install exact versions from package-lock.json
npm run dev          # dev server with hot reload, http://localhost:5173
npm run build        # type check + production build into dist/
npm test             # Vitest unit tests (npm run test:watch while developing)
npm run test:e2e     # end-to-end test, needs a running simcore (see below)
npm run lint         # ESLint (type-aware rules)
npm run format       # Prettier (write); format:check only checks
```

## Live or fake

The cockpit starts with the source chosen last (switch in the bottom bar), `?source=fake` or
`?source=live` in the URL overrides it. Live mode connects to `ws://127.0.0.1:47800`; another
address goes into `ui/.env.local` as `VITE_SIMCORE_URL=ws://127.0.0.1:47801`.

```bash
./build/debug/core/simcore        # from the repository root, in a second terminal
cd ui && npm run test:e2e         # retard ignition by 5 deg, check that torque drops
```

Tunes are saved and loaded with the buttons at the top right (JSON, ADR 0006).

## Structure (iterations 5 and 6, ADR 0005/0006)

| Folder          | Content                                                                                   |
| --------------- | ----------------------------------------------------------------------------------------- |
| `src/protocol/` | Types of protocol v1 (`docs/interface.md`)                                                |
| `src/sim/`      | Port `SimSource`; `fake/` = fake simcore (engine, dyno, protocol) built on `shared/` data |
| `src/domain/`   | Pure logic: table interpolation, selection, edits, undo, smoothing, units                 |
| `src/state/`    | `SimStore` (snapshot for React, 60 Hz state outside React), dyno runs                     |
| `src/ui/`       | Components: gauges (SVG), table editor (canvas), dyno (uPlot), controls, theme            |

Table editor keys: arrows (Shift = extend), `+`/`-`, PageUp/PageDown (5 steps), `S` smooth,
`I` interpolate corners, Ctrl+Z / Ctrl+Y, Ctrl+A. Hold Space anywhere for full throttle.
