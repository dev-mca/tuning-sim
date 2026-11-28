"""Wall heat transfer after Woschni, simplified (docs/research.md §7.3)."""

from tuning_lab.units import pa_to_kpa

WOSCHNI_CONSTANT = 3.26
"""Leading constant of h_c in W/(m^2 K) with B in m, p in kPa, T in K, w in m/s (§7.3)."""


def woschni_coefficient_w_m2_k(
    bore_m: float, pressure_pa: float, temperature_k: float, gas_velocity_m_s: float
) -> float:
    """h_c = 3.26 B^-0.2 p^0.8 T^-0.55 w^0.8 with p in kPa (research.md §7.3).

    Simplified as in research.md: w = C1 * v_p only, without Woschni's combustion term
    C2 V_d T_r / (p_r V_r) (p - p_mot). That term would add heat loss during combustion;
    its effect is discussed in docs/adr/0010 [prüfen].
    """
    return (
        WOSCHNI_CONSTANT
        * bore_m**-0.2
        * pa_to_kpa(pressure_pa) ** 0.8
        * temperature_k**-0.55
        * gas_velocity_m_s**0.8
    )
