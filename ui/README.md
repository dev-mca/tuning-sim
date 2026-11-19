# tuning-sim UI

Cockpit UI for tuning-sim: TypeScript (strict), React, Vite, Tailwind CSS v4.
Talks to `simcore` via WebSocket on `127.0.0.1` (from iteration 6; see `../docs/plan.md`).

```bash
cd ui
npm ci               # install exact versions from package-lock.json
npm run dev          # dev server with hot reload, http://localhost:5173
npm run build        # type check + production build into dist/
npm test             # Vitest unit tests (npm run test:watch while developing)
npm run lint         # ESLint (type-aware rules)
npm run format       # Prettier (write); format:check only checks
```

## Structure (iteration 5, ADR 0005)

| Folder | Content |
|---|---|
| `src/protocol/` | Types of protocol v1 (`docs/interface.md`) |
| `src/sim/` | Port `SimSource`; `fake/` = fake simcore (engine, dyno, protocol) built on `shared/` data |
| `src/domain/` | Pure logic: table interpolation, selection, edits, undo, smoothing, units |
| `src/state/` | `SimStore` (snapshot for React, 60 Hz state outside React), dyno runs |
| `src/ui/` | Components: gauges (SVG), table editor (canvas), dyno (uPlot), controls, theme |

Table editor keys: arrows (Shift = extend), `+`/`-`, PageUp/PageDown (5 steps), `S` smooth,
`I` interpolate corners, Ctrl+Z / Ctrl+Y, Ctrl+A. Hold Space anywhere for full throttle.
