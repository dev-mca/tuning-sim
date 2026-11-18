# tuning-sim UI

Cockpit UI for tuning-sim: TypeScript (strict), React, Vite, Tailwind CSS v4.
Talks to `simcore` via WebSocket on `127.0.0.1` (from iteration 6; see `../docs/plan.md`).

```bash
cd ui
npm ci               # install exact versions from package-lock.json
npm run dev          # dev server with hot reload, http://localhost:5173
npm run build        # type check + production build into dist/
npm run lint         # ESLint (type-aware rules)
npm run format       # Prettier (write); format:check only checks
```
