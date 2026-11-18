"""Well-known repository paths (the lab is installed in editable mode, see lab/README.md)."""

from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parents[3]
SHARED_DIR = REPO_ROOT / "shared"
ENGINES_DIR = SHARED_DIR / "engines"
GOLDEN_DIR = SHARED_DIR / "golden"
LAB_OUTPUT_DIR = REPO_ROOT / "lab" / "output"


def engine_path(engine_id: str) -> Path:
    return ENGINES_DIR / f"{engine_id}.json"
