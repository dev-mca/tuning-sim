"""Cylinder-resolved 0D engine model, prototype for iteration 11 (docs/research.md §7).

Unlike the mean-value model (`tuning_lab.mvm`, research.md §2), this package integrates one
cylinder over crank angle: volume from the slider-crank (§1.4), first law for the combustion
chamber (§7.1), Wiebe heat release (§7.2), Woschni wall heat (§7.3) and isentropic valve flow
(§7.4). Intake and exhaust are constant-pressure reservoirs. All cylinders of an evenly firing
engine are identical, so the other cylinders are the same cycle shifted by their firing angle.

Modules:
- `parameters`    assumed data the engine JSON does not have (valves, Wiebe, wall temperature)
- `kinematics`    cylinder volume, dV/dtheta, wall area, mean piston speed (§1.4)
- `gas`           temperature-dependent gas properties u(T), h(T), gamma(T)
- `valves`        lift curves and orifice flow through the valves (§7.4, §2.5)
- `combustion`    Wiebe burned fraction and burn duration (§7.2)
- `heat_transfer` simplified Woschni coefficient (§7.3)
- `cylinder`      the cycle integration (RK4 over crank angle, §9)
- `engine`        engine-level results: IMEP, torque, VE, EGT, exhaust flow of all cylinders
- `validation`    comparison with the MVM at the same operating points
- `report`        plots to lab/output/zero_d/ (python -m tuning_lab.zero_d.report)
"""
