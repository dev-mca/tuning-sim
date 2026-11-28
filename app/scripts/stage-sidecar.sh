#!/bin/sh
# Copies the CMake-built simcore to app/src-tauri/binaries/simcore-<target-triple>.
#
# Why: Tauri's bundle.externalBin expects each sidecar with the Rust target triple as suffix
# (e.g. simcore-x86_64-unknown-linux-gnu). simcore is built by CMake, not by Cargo, so this
# script bridges the two builds without touching core/. tauri.conf.json runs it before every
# `tauri dev` and `tauri build`; run it by hand before a plain `cargo build`/`cargo clippy`.
#
# SIMCORE_BIN overrides the source binary (default: <repo>/build/release/core/simcore).
set -eu

script_dir=$(cd "$(dirname "$0")" && pwd)
repo_root=$(cd "$script_dir/../.." && pwd)
source_bin=${SIMCORE_BIN:-"$repo_root/build/release/core/simcore"}
binaries_dir="$script_dir/../src-tauri/binaries"

if [ ! -x "$source_bin" ]; then
    echo "error: simcore not found at $source_bin" >&2
    echo "build it first (from the repo root): cmake --preset release && cmake --build --preset release" >&2
    exit 1
fi

# rustc >= 1.84 prints the host triple directly; older versions only via -vV.
target_triple=$(rustc --print host-tuple 2>/dev/null || rustc -vV | sed -n 's/^host: //p')
if [ -z "$target_triple" ]; then
    echo "error: cannot determine the Rust target triple (is rustc on PATH?)" >&2
    exit 1
fi

mkdir -p "$binaries_dir"
install -m 755 "$source_bin" "$binaries_dir/simcore-$target_triple"
echo "staged $source_bin -> src-tauri/binaries/simcore-$target_triple"
