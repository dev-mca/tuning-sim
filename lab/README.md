# tuning-lab

Python calculation lab for tuning-sim: reference implementations of the physics
(see `../docs/research.md`), plots, and golden files in `../shared/golden/` that the
C++ tests in `core/` compare against. Not a second simulator.

```bash
cd lab
python3 -m venv .venv
.venv/bin/pip install -e ".[dev]"
.venv/bin/pytest
.venv/bin/ruff check . && .venv/bin/ruff format --check .
```
