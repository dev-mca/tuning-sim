# ADR 0010 – Zylinderaufgelöstes 0D-Modell (Lab-Prototyp)

- Status: angenommen (2026-11-28, vorher vorgeschlagen)
- Nachtrag beim Merge: Als Prototyp im Lab. Entscheide zu den offenen Fragen: Woschni-Verbrennungsterm bleibt optional (Standard wie research.md §7.3); Ventildaten bleiben Annahmen im Modul, bis der C++-Port kommt; die MVM-VE-Tabelle und das Golden File bleiben unverändert. Gasdynamik (§7.6) ist der nächste Schritt für Iteration 11.
- Datum: 2026-11-28

## Kontext

Iteration 11 (plan.md) verlangt pro Zylinder Volumen über Kurbelwinkel, Wiebe-Brennverlauf,
Druck/Temperatur, Ventilströmung und den Auslassmassenstrom als künftige Schallquelle, dazu einen
Vergleich MVM vs. 0D im Lab. Physik zuerst im Lab (Learning 2026-11-18), C++-Port später. Dieses
ADR beschreibt den Python-Prototyp `lab/src/tuning_lab/zero_d/` und welche Annahmen der Nutzer
noch bestätigen soll.

## Entscheide

1. **Ein Zylinder, die anderen phasenverschoben.** Mit Saugrohr und Auspuff als Behältern
   konstanten Drucks laufen alle Zylinder eines gleichmässig zündenden Motors identisch. Simuliert
   wird Zylinder 1, die übrigen sind um `firingAngles_deg` verschoben (§1.2). Mit 1D-Rohren oder
   gemeinsamem Krümmervolumen fällt diese Vereinfachung weg.
2. **Zustand und Gleichungen** (§7.1): Masse m, innere Energie U = m·u(T) und Frischgemischmasse
   pro Zylinder. U direkt zu integrieren hält den ersten Hauptsatz exakt. Ein Arbeitsspiel läuft
   von ES (IVC) bis ES, damit die eingeschlossene Ladung und damit die Kraftstoffmasse vor der
   Verbrennung bekannt sind. Arbeitsspiele werden wiederholt, bis eingeschlossene Masse,
   Abgastemperatur und Abgasgegendruck um weniger als 1e-6 schwanken (typisch 6–8 Spiele).
3. **Gas** [Annahme]: ein ideales Gas mit R von Luft und c_v(T) = 850 + 0,12·T J/(kg·K), angepasst
   an die Einzonen-Korrelation γ(T) = 1,338 − 6,0e-5·T + 1,0e-8·T² (Brunt, SAE 981052) [prüfen]:
   γ ≈ 1,32 bei 300 K, 1,25 bei 2500 K. Konstantes γ = 1,4 überschätzt die Expansionsarbeit.
4. **Wiebe** (§7.2): a = 5, m = 2, auf 0 → 1 normiert. Zündverzug 5° [Annahme]. Die Brenndauer
   wird aus der MBT-Tabelle abgeleitet: Zündung bei MBT ⇒ 50 % verbrannt bei 8° n. OT (Faustregel,
   Heywood Kap. 9) [Annahme]. Ergebnis 35–75° (§7.2: 40–70°), lange Brenndauer bei Teillast. So
   bleibt das beste Moment des 0D-Modells nahe an der MBT des MVM (Test: Optimum ±6° um MBT).
5. **Wandwärme** (§7.3): Woschni vereinfacht wie in research.md, w = C₁·v̄_p (C₁ = 2,28 geschlossen,
   6,18 bei offenem Ventil), T_Wand = 450 K, flacher Kopf und Kolben [Annahme]. Woschnis
   Verbrennungsterm C₂·V_d·T_r/(p_r·V_r)·(p − p_mot) ist als Option eingebaut
   (`HeatTransferAssumptions.combustion_term`), standardmässig aus (siehe offene Fragen).
6. **Ventile** (§7.4) [Annahme, nicht in der Motor-JSON]: je 2 Einlass- (32 mm, Hub 9,5 mm) und
   Auslassventile (28 mm, Hub 9,0 mm), sin²-Hubkurve zwischen den Steuerzeiten der JSON (als Punkte
   mit Hub 0 gelesen), Fläche π·d·L begrenzt durch den Sitzquerschnitt, C_d = 0,65 konstant.
   Strömung mit der Blendengleichung aus §2.5; Ψ ist die bestehende Funktion aus `intake.py` (mit
   ihrem linearen Stück bei Π > 0,99, das die Strömung bei fast gleichen Drücken nicht steif macht).
7. **Rand** [Annahme]: Saugrohr = Ladedruck des Betriebspunkts (MAP) und 25 °C; Auspuff =
   Umgebung + Gegendruckgesetz der Motor-JSON (wie im MVM), Abgas-Rückströmung mit der mittleren
   Abgastemperatur. **Rückströmung ins Saugrohr** wartet in einem Puffer pro Zylinder (ohne
   Volumen, adiabat) und strömt zuerst zurück. Ohne diesen Puffer würde die Überschneidung den
   Zylinder mit Frischgemisch spülen: Restgas im Leerlauf 4 % statt 11 %, Luftmasse bei Teillast
   bis +55 %. Die letzten 1 % einer Ladung im Puffer mischen sich weich mit Frischgemisch, sonst
   entsteht durch das harte Umschalten im RK4-Schritt ein kleiner Grenzzyklus.
8. **Kraftstoff**: Saugrohreinspritzung, das Gemisch strömt mit λ des Betriebspunkts ein
   (Vergleich: λ-Ziel der Serien-ECU, als hätte die Lambdaregelung auf die 0D-Luftmasse
   nachgeregelt). Verbrannt wird min(m_fuel, m_air/AFR_st) (§2.6), η_comb = 0,98 [Annahme].
9. **Numerik** (§9): klassisches RK4 mit festem Schritt in °KW. Schritt = min(1°, ω·25 µs),
   also 1° bei 7000/min und 0,15° bei 1000/min. Grund für die Zeitgrenze: Die Ventilströmung
   bei offenem Ventil hat eine Zeitkonstante von ~70 µs; RK4 ist bis h·λ ≈ 2,8 stabil, also bis
   ~190 µs. Bei 1000/min und 4° (0,67 ms) wird das Verfahren instabil und bricht mit
   `FloatingPointError` ab (Test). Die Brennrate ist pro Schritt der exakte Mittelwert
   (x_b(θ_{i+1}) − x_b(θ_i))/h, damit die freigesetzte Energie exakt stimmt (RK4 würde den Knick
   am Brennende mit ~1e-4 Fehler integrieren). Deterministisch, keine adaptive Schrittweite: gut
   für den C++-Port und bitgenaue Golden Files.

## Validierung gegen das MVM

Gleicher Betriebspunkt (Drehzahl, MAP, Zündung und λ der Serien-ECU). Volllast-MAP aus dem
MVM-Gleichgewicht Drosselklappe = Motorluft. `python -m tuning_lab.zero_d.report` (Plots in
`lab/output/zero_d/`). T_i,g = indiziert ohne Ladungswechsel, T_e = effektiv (0D: netto indiziert −
MVM-Reibung − Nebenaggregate).

| 1/min | MAP kPa | T_i MVM | T_i,g 0D | T_e MVM | T_e 0D | ΔT_e | Luft MVM mg | Luft 0D mg | ΔLuft | VE 0D | AGT 0D °C | p_max bar |
|---|---|---|---|---|---|---|---|---|---|---|---|---|
| 1000 | 101,2 | 270,6 | 297,5 | 249,3 | 275,8 | +10,7 % | 2918 | 2987 | +2,4 % | 0,844 | 992 | 60,2 |
| 2000 | 101,0 | 298,1 | 300,4 | 273,7 | 274,8 | +0,4 % | 3228 | 3032 | −6,1 % | 0,859 | 1020 | 54,7 |
| 3000 | 100,7 | 319,0 | 299,6 | 290,7 | 268,7 | −7,6 % | 3447 | 3027 | −12,2 % | 0,859 | 1033 | 53,6 |
| 4000 | 100,5 | 333,6 | 296,4 | 300,6 | 258,9 | −13,9 % | 3594 | 2995 | −16,7 % | 0,852 | 1041 | 53,7 |
| 5000 | 100,3 | 341,0 | 290,8 | 302,5 | 245,6 | −18,8 % | 3669 | 2943 | −19,8 % | 0,839 | 1052 | 53,3 |
| 6000 | 99,8 | 337,5 | 283,6 | 292,9 | 229,6 | −21,6 % | 3628 | 2872 | −20,9 % | 0,822 | 1062 | 52,7 |
| 7000 | 99,5 | 318,3 | 275,6 | 267,3 | 212,1 | −20,7 % | 3420 | 2791 | −18,4 % | 0,802 | 1072 | 52,1 |
| 800 | 35 | 47,8 | 72,0 | 11,2 | 35,4 | +216 % | 661 | 850 | +28,6 % | 0,695 | 1106 | 9,6 |
| 2000 | 50 | 107,0 | 135,9 | 70,7 | 99,2 | +40 % | 1192 | 1386 | +16,2 % | 0,793 | 1026 | 26,9 |
| 3000 | 60 | 146,9 | 168,6 | 109,5 | 129,9 | +19 % | 1636 | 1719 | +5,0 % | 0,819 | 1042 | 32,6 |
| 4000 | 40 | 89,6 | 101,8 | 43,6 | 54,6 | +25 % | 999 | 1058 | +6,0 % | 0,757 | 1078 | 20,4 |

Einordnung (research.md §10):
- **Volllast plausibel:** Spitzenmoment 276 Nm (bmep 11,6 bar; §10: 11–13,5 bar, 280–330 Nm knapp
  darunter), p_max 52–60 bar bei 14–18° n. OT, indizierter Wirkungsgrad (brutto) 0,43, Restgas
  4–5,5 %, Pumpverlust ≈ p_exh − p_m wie im MVM (Test: ±15 kPa).
- **Hohe Drehzahl: −19 bis −23 %.** Die VE des 0D-Modells fällt von 0,86 auf 0,80, die VE-Tabelle
  steigt bis 1,05 bei 5500/min. Konstanter Druck vor den Ventilen kennt keine Gasträgheit
  (Nachladeeffekt beim späten ES, Saugrohr-Resonanz). Spitzenleistung daher 155 kW statt 196 kW
  (§10: 190–230 kW).
- **Teillast: 0D mit mehr Luft (+5 bis +29 %).** Die VE-Tabelle ist bei tiefem MAP deutlich tiefer
  (0,54 bei 35 kPa) als das 0D-Modell (0,70). Weil das effektive Moment bei Teillast eine kleine
  Differenz ist, sind die relativen Abweichungen dort gross (Leerlauf +216 %, absolut +24 Nm).
  Das 0D-Modell behandelt die Spätzündung im Leerlauf (4° statt MBT 24°) physikalisch, das MVM
  über η_ign.
- **Abgastemperatur zu hoch:** 990–1070 °C (§10: 700–950 °C). Gemessen wird die enthalpie-
  gemittelte Temperatur am Ventil ohne Wärmeverlust im Auslasskanal. Die Wandwärme ist mit dem
  vereinfachten Woschni klein (10–12 % der Brennstoffenergie bei Volllast).
- **Woschni mit Verbrennungsterm** (Option): Wandwärme 12–24 %, Wirkungsgrad 0,39–0,43, AGT
  830–1050 °C, Volllastmoment −3 bis −12 %, also näher am MVM (0,40) und an §10.
- **Rechenzeit:** ≈ 0,2 s pro Betriebspunkt in Python (6–8 Arbeitsspiele, 720–4800 RK4-Schritte).
  Konvergenz: 2° gegenüber 0,125° ändert die Arbeit um 0,02 % (7000/min) bzw. 0,01 % (3000/min).

## Was für den C++-Port und die Sound-Kopplung noch fehlt

- **Echtzeit statt Zyklus-Iteration:** simcore braucht einen fortlaufenden Zustand mit festem
  Zeitschritt (z. B. Audio-Takt 48 kHz ≈ 21 µs, unter der 25-µs-Grenze), alle 6 Zylinder
  einzeln und Drehzahländerungen innerhalb des Spiels. Kraftstoff und Zündung pro Zylinder bei
  ES aus der ECU.
- **Gasdynamik zwischen Ventil und Behältern:** ohne Rohre (1D, §7.6) oder wenigstens Saugrohr-
  und Krümmervolumen mit Gasträgheit fehlt die Resonanzaufladung. Die Abnahme von Iteration 11
  («Krümmerlänge verändert … messbar die Drehmomentkurve») ist mit konstanten Drücken nicht
  erreichbar.
- **Kopplung an den Wellenleiter** (§7.5): ṁ_out,k(θ) liegt pro Zylinder vor
  (`ZeroDEngineResult.exhaust_mass_flow_by_cylinder_kg_s`); offen ist die Umrechnung in eine
  Druck-/Schnellequelle (z. B. p = ρ·c·u mit u = ṁ/(ρ·A_Rohr)) und die Rückwirkung des
  Wellenleiters auf den Ventil-Gegendruck.
- **Golden File:** Ein Betriebspunkt (p-θ, ṁ_out) als CSV für den bitgenauen C++-Vergleich,
  sobald die Annahmen bestätigt sind.
- **Motor-JSON** (Vorschlag, nicht umgesetzt, `shared/` gehört nicht zu diesem Paket):
  `valves: { intake: {count, diameter_m, maxLift_m, dischargeCoefficient}, exhaust: {…} }`
  und optional `wallTemperature_K`. Die Ventilsteuerzeiten und die Pleuellänge sind schon da;
  `engine_definition.py` liest sie noch nicht, das 0D-Paket liest sie selbst.

## Offene Fragen an den Nutzer

1. Woschni mit Verbrennungsterm als Standard? Realistischere Wandwärme und AGT, aber über
   research.md §7.3 («vereinfacht») hinaus.
2. Nächster Schritt für die Abnahme «Krümmerlänge»: einfache Rohre mit Gasträgheit (Helmholtz/
   Plenum + Rohr als Masse-Feder) oder gleich 1D-Finite-Volumen (§7.6)?
3. Ventildaten in die Motor-JSON aufnehmen (Vorschlag oben)? Die Werte sind geschätzt.
4. Soll die MVM-VE-Tabelle bei Teillast an das 0D-Modell angenähert werden (0,54 → ~0,70 bei
   35 kPa)? Das würde das Golden File und den Leerlauf-Abgleich ändern.
5. Brenndauer aus der MBT-Tabelle ableiten (heute) oder eine eigene Brenndauer-Tabelle über
   Drehzahl und Last? Ersteres hält beide Modelle konsistent, letzteres ist unabhängiger.

## Konsequenzen

- (+) Physikalisch begründete Druckverläufe, Ladungswechsel und Abgaspulse pro Zylinder; der
  Auslassmassenstrom ist bereit als Schallquelle.
- (+) Volllast im Bereich des MVM bis 4000/min (±14 %), überall plausibel nach §10 (bis auf
  AGT und Leistung, beide begründet).
- (+) 58 Tests: Kinematik, Gas, Ventile, Wiebe, Schleppbetrieb (Symmetrie, Polytropenexponent),
  Energiebilanz, Massenerhaltung, Determinismus, Schrittweiten-Konvergenz, MVM-Vergleich.
- (−) Viele geschätzte Parameter (Ventile, Wandtemperatur, Zündverzug, CA50 bei MBT).
- (−) Ohne Gasdynamik fehlt die Drehmomentkurve des echten Saugmotors bei hoher Drehzahl.
