"""Shared pytest fixtures (pytest loads conftest.py automatically)."""

import pytest

from tuning_lab.dyno import SweepResult, run_sweep
from tuning_lab.engine_definition import EngineDefinition, load_engine
from tuning_lab.paths import engine_path


@pytest.fixture(scope="session")
def i6() -> EngineDefinition:
    """The reference engine, loaded once per test run."""
    return load_engine(engine_path("i6_30_na"))


@pytest.fixture(scope="session")
def stock_sweep(i6: EngineDefinition) -> SweepResult:
    """Full-load sweep 1000-7000 1/min with the stock tune (about 1 s, shared by tests)."""
    return run_sweep(i6, 1000.0, 7000.0)
