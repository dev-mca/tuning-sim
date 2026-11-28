#!/bin/sh
# Runs before `tauri dev` / `tauri build` (beforeDevCommand / beforeBuildCommand in
# tauri.conf.json): stages the simcore sidecar, then starts the Vite dev server or builds the UI.
#
# The UI reads the simcore address at build time (VITE_SIMCORE_URL, ui/src/sim/createSource.ts).
# The shell reads TUNING_SIM_PORT at run time. Deriving one from the other keeps both on the
# same port.
#
# Usage: prepare-ui.sh dev|build
set -eu

script_dir=$(cd "$(dirname "$0")" && pwd)
ui_dir="$script_dir/../../ui"
# Not 5173: the user's own `npm run dev` often runs there (CLAUDE.md, Learnings).
dev_server_port=5180

sh "$script_dir/stage-sidecar.sh"

VITE_SIMCORE_URL="ws://127.0.0.1:${TUNING_SIM_PORT:-47800}"
export VITE_SIMCORE_URL

case "${1:-}" in
    dev)
        # --strictPort: fail instead of silently moving to another port that devUrl does not know.
        exec npm --prefix "$ui_dir" run dev -- --port "$dev_server_port" --strictPort
        ;;
    build)
        exec npm --prefix "$ui_dir" run build
        ;;
    *)
        echo "usage: $0 dev|build" >&2
        exit 2
        ;;
esac
