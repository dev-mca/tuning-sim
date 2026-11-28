"""Fixtures for the 0D model tests: engine and a few operating points, computed once."""

import pytest

from tuning_lab import units
from tuning_lab.engine_definition import EngineDefinition
from tuning_lab.zero_d.cylinder import ZeroDOperatingPoint
from tuning_lab.zero_d.engine import ZeroDEngineResult, run_operating_point
from tuning_lab.zero_d.parameters import ZeroDEngine, load_zero_d_engine
from tuning_lab.zero_d.validation import full_load_manifold_pressure_pa

FULL_LOAD_RPM = 4000.0


def stock_point(
    definition: EngineDefinition, speed_rpm: float, manifold_pressure_pa: float
) -> ZeroDOperatingPoint:
    """Operating point with the stock ignition and lambda tables."""
    speed = units.rpm_to_rad_s(speed_rpm)
    return ZeroDOperatingPoint(
        speed_rad_s=speed,
        intake_pressure_pa=manifold_pressure_pa,
        ignition_deg=definition.tables.ignition.lookup(speed, manifold_pressure_pa),
        lambda_value=definition.tables.lambda_target.lookup(speed, manifold_pressure_pa),
    )


@pytest.fixture(scope="session")
def zero_d_engine() -> ZeroDEngine:
    return load_zero_d_engine()


@pytest.fixture(scope="session")
def full_load_point(zero_d_engine: ZeroDEngine) -> ZeroDOperatingPoint:
    definition = zero_d_engine.definition
    manifold = full_load_manifold_pressure_pa(definition, units.rpm_to_rad_s(FULL_LOAD_RPM))
    return stock_point(definition, FULL_LOAD_RPM, manifold)


@pytest.fixture(scope="session")
def full_load(
    zero_d_engine: ZeroDEngine, full_load_point: ZeroDOperatingPoint
) -> ZeroDEngineResult:
    """4000 1/min wide-open throttle with the stock tune."""
    return run_operating_point(zero_d_engine, full_load_point)
