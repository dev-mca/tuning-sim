# research.md – Physik, Mathematik und Quellen

Dieses Dokument erklärt die Theorie hinter tuning-sim: wie ein Motor Luft und Kraftstoff in Drehmoment verwandelt, wie eine ECU das steuert, wie Turbo, Kompressor und Antriebsstrang funktionieren und wie physikalisch begründeter Motorsound entsteht. Code verweist auf die Abschnittsnummern (z. B. `// see research.md §2.4`).

**Grundsätze**
- Intern gelten SI-Einheiten: m, kg, s, Pa, K, J, W, rad, rad/s.
- Die Modelle sind bewusst vereinfacht («Mean-Value» und «0D»). Sie sollen Zusammenhänge richtig zeigen, nicht Messwerte auf das Prozent genau treffen.
- Zahlenwerte mit ≈ sind typische Grössenordnungen für die ersten Tests. Sie ersetzen keine Quelle.
- Mit **[prüfen]** markierte Stellen sollen vor der Implementierung in der Originalquelle nachgeschlagen werden.

---

## 0. Formelzeichen und Konstanten

| Symbol | Bedeutung | Einheit / typischer Wert |
|---|---|---|
| N | Motordrehzahl | 1/min |
| ω | Winkelgeschwindigkeit, ω = 2πN/60 | rad/s |
| θ | Kurbelwinkel | rad bzw. °KW |
| V_d | Hubraum gesamt | m³ (3,0 l = 0,003 m³) |
| z | Zylinderzahl | – |
| B, S | Bohrung, Hub | m |
| ε | Verdichtungsverhältnis | – |
| p, T, ρ | Druck, Temperatur, Dichte | Pa, K, kg/m³ |
| R | spez. Gaskonstante Luft | 287,05 J/(kg·K) |
| γ (κ) | Isentropenexponent | Luft 1,40; Abgas ≈ 1,30–1,35 |
| c_p | spez. Wärmekapazität | Luft ≈ 1005 J/(kg·K); Abgas ≈ 1150 J/(kg·K) |
| λ | Luftverhältnis | – |
| AFR_st | stöchiometrisches Luft/Kraftstoff-Verhältnis | Benzin E0 ≈ 14,7; E10 ≈ 14,1 |
| H_u (Q_LHV) | unterer Heizwert Benzin | ≈ 42–44 MJ/kg |
| J | Massenträgheitsmoment | kg·m² |

Umrechnungen nur am Rand: 1 bar = 10⁵ Pa; 1 kPa = 10³ Pa; 1 PS = 735,5 W; 1 hp = 745,7 W; T[K] = T[°C] + 273,15.

---

## 1. Viertakt-Grundlagen

### 1.1 Arbeitsspiel
Ein Viertaktmotor braucht **zwei Umdrehungen (720 °KW)** pro Arbeitsspiel: Ansaugen, Verdichten, Arbeiten, Ausstossen. Jeder Zylinder zündet also einmal pro 720°.

### 1.2 Zündabstand und Zündfrequenz
- Gleichmässiger Zündabstand: Δθ = 720° / z. I4: 180°, I6: 120°, V8: 90°.
- Zündfrequenz (Grundfrequenz des Auspufftons): **f_z = (N/60) · z/2**.
  Beispiel I6 bei 3000/min: 50 · 3 = **150 Hz**.
- In der Akustik spricht man von **Motorordnungen** (Vielfache der Drehfrequenz N/60). Die dominierende Ordnung eines gleichmässig zündenden Viertakters ist **z/2**: I4 → 2. Ordnung, I6 → 3., V8 → 4. Ordnung.

### 1.3 Zündfolgen (Beispiele)
| Motor | Zündfolge | Abstände |
|---|---|---|
| I4 | 1-3-4-2 | 180° gleichmässig |
| I6 | 1-5-3-6-2-4 | 120° gleichmässig |
| V8 Crossplane (z. B. GM LS) | 1-8-7-2-6-5-4-3 | 90° gesamt, **pro Bank ungleichmässig** |
| V8 Flatplane | 1-5-4-8-3-7-2-6 (je nach Hersteller) | 90° gesamt, pro Bank gleichmässig 180° |
| Boxer 4 (Subaru EJ) | 1-3-2-4 | 180°, aber ungleich lange Krümmer |

**Warum der Crossplane-V8 «blubbert»:** Gesamtmotor zündet gleichmässig alle 90°. Jede Zylinderbank (und damit jeder Krümmer bei getrennten Bänken) bekommt die Pulse aber in ungleichmässigen Abständen. Dadurch entstehen starke **halbe Ordnungen** und tiefe Modulationen. Beim Flatplane-V8 ist jede Bank wie ein I4 und klingt heller, eher «kreischend». Das ist der Hauptgrund, warum das Akustikmodell **pro Zylinder und pro Krümmerrohr** arbeiten muss.

### 1.4 Kurbeltriebkinematik
Kurbelradius a = S/2, Pleuellänge l. Kolbenweg ab OT:

  s(θ) = a + l − ( a·cos θ + √(l² − a²·sin²θ) )

Zylindervolumen:

  V(θ) = V_c + (π B² / 4) · s(θ), mit Kompressionsvolumen V_c = V_h / (ε − 1), V_h = π B² S / 4.

Mittlere Kolbengeschwindigkeit: **v̄_p = 2 · S · N / 60** (Serienmotoren bei Nenndrehzahl ≈ 15–22 m/s).

Quelle: Heywood, Kap. 2.

---

## 2. Mean-Value-Motormodell (MVM)

Ein MVM mittelt über ein Arbeitsspiel. Es kennt keine einzelnen Zündungen, liefert aber Drehmoment, Luftstrom und Drücke als glatte Grössen. Für ECU, Dyno, Turbo und Regler ist das ideal und schnell (1 kHz reicht).

### 2.1 Ansaugdichte (ideales Gas)
  ρ_m = p_m / (R · T_m)

p_m: Saugrohrdruck (MAP), T_m: Saugrohrtemperatur.

### 2.2 Luftmasse und Luftmassenstrom (Speed Density)
Luftmasse pro Arbeitsspiel (ganzer Motor):

  m_air,cyc = η_v · ρ_m · V_d

Luftmassenstrom (Viertakt, ein Arbeitsspiel pro zwei Umdrehungen):

  ṁ_air = η_v · ρ_m · V_d · N / 120     (N in 1/min)

η_v ist der **Liefergrad (Volumetric Efficiency, VE)**, aus der VE-Tabelle über (N, MAP). Typisch 0,80–1,05 bei Saugmotoren, Spitzen im Bereich der Ansaugresonanz. Unter Aufladung bezogen auf Umgebungsdruck sind Werte > 100 % normal, bei Speed Density mit MAP-Bezug liegen sie aber ähnlich wie beim Sauger.

Diese Gleichung ist **genau das**, was eine Speed-Density-ECU (Speeduino, rusEFI) rechnet, um die Einspritzmenge zu bestimmen. Darum ist die VE-Tabelle die wichtigste Kraftstofftabelle.

### 2.3 Tabelleninterpolation (bilinear)
Gegeben Achsen x₀ < x₁ < … und y₀ < y₁ < …, gesucht f(x, y):
1. Bins finden: i mit x_i ≤ x ≤ x_{i+1}, j mit y_j ≤ y ≤ y_{j+1} (binäre Suche; ausserhalb klemmen auf den Rand).
2. Normierte Anteile: t = (x − x_i)/(x_{i+1} − x_i), u = (y − y_j)/(y_{j+1} − y_j).
3. f = (1−t)(1−u)·f_{i,j} + t(1−u)·f_{i+1,j} + (1−t)u·f_{i,j+1} + t·u·f_{i+1,j+1}

Eigenschaften, die getestet werden: exakte Werte an Stützstellen, Linearität entlang einer Achse, Klemmung ausserhalb, Stetigkeit über Bin-Grenzen.

### 2.4 Kraftstoff
  m_fuel = m_air / (λ_target · AFR_st)

Die ECU spritzt nach λ_target. Das «tatsächliche» λ im Modell kann davon abweichen, wenn die VE-Tabelle der ECU nicht zur «wahren» VE des Motors passt. **Didaktischer Trick:** Das Modell hat eine *wahre* VE (Motoreigenschaft) und die ECU eine *geglaubte* VE (Tabelle). Stimmen sie nicht überein, zeigt die Lambdasonde ein anderes λ als das Ziel, und genau so tunt man im echten Leben die VE-Tabelle.

  λ_actual = m_air,true / (m_fuel · AFR_st)

### 2.5 Saugrohr und Drosselklappe
Füll- und Entleermodell (Behälter mit Volumen V_m, isotherm):

  dp_m/dt = (R · T_m / V_m) · ( ṁ_thr − ṁ_air )

Massenstrom durch die Drosselklappe als kompressible Blende:

  ṁ_thr = C_d · A_thr(α) · p_0 / √(R T_0) · Ψ(p_m / p_0)

mit

  Ψ(Π) = √( 2γ/(γ−1) · ( Π^{2/γ} − Π^{(γ+1)/γ} ) )   für Π > Π_krit
  Ψ = √( γ · (2/(γ+1))^{(γ+1)/(γ−1)} )                für Π ≤ Π_krit (gesperrte Strömung)
  Π_krit = (2/(γ+1))^{γ/(γ−1)} ≈ 0,528 für Luft

Offene Fläche einer Klappe mit Durchmesser D und Winkel α (geschlossen bei α₀): A_thr ≈ (π D²/4) · (1 − cos α / cos α₀). Dazu eine kleine Leckfläche für den Leerlauf. Bei Aufladung ist p_0 der Ladedruck nach dem Ladeluftkühler.

Quelle: Guzzella & Onder, Kap. 2.3; Heywood, Anhang C.

### 2.6 Indiziertes Moment
Indizierte Arbeit pro Arbeitsspiel:

  W_i = m_fuel,burned · H_u · η_i,base · η_ign(Δθ) · η_λ(λ)

Indiziertes Moment (Viertakt): **T_i = W_i / (4π)**

- **η_i,base**: indizierter Wirkungsgrad bei MBT und λ ≈ 1. Theoretische Obergrenze Otto-Prozess η_Otto = 1 − ε^{1−γ} (für ε = 10, γ = 1,3: ≈ 0,50). Realistisch für das Modell: η_i,base ≈ 0,36–0,40.
- **Verbrannte Kraftstoffmasse**: Bei λ < 1 fehlt Sauerstoff, verbrannt wird höchstens m_air / AFR_st. Also m_fuel,burned = min(m_fuel, m_air / AFR_st).
- **η_λ(λ)**: Feinkorrektur. Leicht fett (λ ≈ 0,85–0,90) gibt durch Ladungskühlung und schnellere Verbrennung das maximale Moment; mager (λ > 1,1) fällt der Wirkungsgrad wegen langsamer Verbrennung. Für den Start eine glatte Kurve (z. B. Polynom oder Tabelle) mit Maximum ≈ 1,0 bei λ ≈ 0,88. **[prüfen]** Guzzella & Onder geben einen Ansatz mit Polynom in λ.
- **η_ign(Δθ)** siehe 2.7.

### 2.7 Zündwinkel und MBT
MBT (Minimum advance for Best Torque) ist der Zündwinkel mit maximalem Moment bei gegebenem Betriebspunkt. Abweichungen kosten Moment, näherungsweise quadratisch:

  η_ign = max(0, 1 − k_ign · (θ_ign − θ_MBT)²)

Mit Δθ in °KW ist k_ign ≈ 4·10⁻⁴ bis 7·10⁻⁴ 1/°² ein brauchbarer Startwert (10° spät kosten ca. 4–7 %). **[prüfen]** Guzzella & Onder §2.4.

Wichtig: Spätzündung verschiebt die Verbrennung in den Expansionstakt. Weniger Arbeit, dafür **heisseres Abgas** (höhere EGT, mehr Turbinenleistung, später lauterer Auspuff). Das Modell erhöht deshalb die Abgasenthalpie, wenn η_ign sinkt (siehe 2.10). Zu frühe Zündung bringt im MVM ebenfalls weniger Moment. Die eigentliche Strafe dafür ist **Klopfen** (§4).

### 2.8 Reibung und Pumpverluste
Reibmitteldruck nach einer gängigen Korrelation für Ottomotoren (Heywood):

  fmep [kPa] ≈ 97 + 15 · (N/1000) + 5 · (N/1000)²   **[prüfen]** Heywood Kap. 13

Alternativ Chen-Flynn: fmep = A + B·p_max + C·v̄_p + D·v̄_p² (benötigt Spitzendruck, eher für das 0D-Modell).

Ladungswechselverluste (Pumpmitteldruck), einfach: pmep ≈ p_exh − p_m (bei Teillast positiv = Verlust; mit Turbo und p_m > p_exh negativ = Gewinn).

Umrechnung Mitteldruck → Moment: **T = mep · V_d / (4π)**

### 2.9 Effektives Moment, Leistung, Plausibilität
  T_e = T_i − T_f − T_p − T_aux     (T_aux: Nebenaggregate, Kompressor §5.5)
  P_e = T_e · ω

Effektiver Mitteldruck **bmep = 4π · T_e / V_d** ist der wichtigste Plausibilitätscheck:
- Saugmotor Serie: 11–13,5 bar
- Turbo Serie: 18–25 bar
- Beispiel 3,0 l Sauger mit 13 bar: T = 13·10⁵ · 0,003 / 12,566 ≈ **310 Nm**.

### 2.10 Abgastemperatur und -enthalpie (grob)
Energiebilanz: Kraftstoffenergie = Arbeit + Kühlung + Abgas. Einfacher Ansatz:

  Q̇_exh = ṁ_fuel · H_u · (1 − η_i,eff − η_cool)
  T_exh = T_m + Q̇_exh / ( (ṁ_air + ṁ_fuel) · c_p,exh )

mit η_i,eff = η_i,base · η_ign · η_λ und η_cool ≈ 0,25–0,30. Ergebnis soll im Bereich 700–950 °C bei Volllast liegen, sonst η_cool anpassen.

### 2.10a Unverbrannter Kraftstoff, Nachverbrennung und Katalysator (Iteration 7b)
Zwei Wege führen Kraftstoff unverbrannt in den Auspuff:
1. **Zu fett** (λ < 1): Es fehlt Sauerstoff, der Überschuss m_fuel − m_air/AFR_st kann auch im Auspuff nicht verbrennen (kein O₂).
2. **Zu spät gezündet:** Liegt die Zündung weit hinter MBT, ist die Verbrennung beim Öffnen des Auslassventils (EVO) noch nicht fertig. Das unverbrannte Gemisch bringt seinen Sauerstoff mit und kann im heissen Krümmer oder Kat **nachverbrennen** (Knall, «Pop»).

Modell für den späten Anteil (grobe, abgestimmte Annahme **[prüfen]**):

  f_late = clamp((Δθ − θ₀) / θ_span, 0, f_max),  Δθ = θ_MBT − θ_ign

mit θ₀ ≈ 25° (bis dahin brennt alles im Zylinder aus; die Leerlauf-Momentenreserve von ~20° darf noch nicht zählen), θ_span ≈ 20°, f_max ≈ 0,6. Nachverbrennende Masse m_ab = f_late · m_fuel,burned. Das MVM-Moment ändert sich dadurch nicht, der Momentenverlust steckt schon in η_ign (§2.7).

**Katalysatortemperatur** als träges Glied erster Ordnung (Wärmekapazität des Kats):

  dT_cat/dt = (T_cat,target − T_cat) / τ_cat,  T_cat,target = T_exh + ΔT_ab
  ΔT_ab = η_ab · m_ab · H_u / (m_exh · c_p,exh)

mit τ_cat ≈ 10 s und η_ab ≈ 0,5 (Anteil, der wirklich nachverbrennt). Während der Schubabschaltung fliesst nur kühle Luft durch, der Kat kühlt ab. Bei kaltem Kat (< ~480 °C) entzündet sich das Gemisch im Auspuff kaum; voll «knallbereit» ist er ab ~630 °C. Das ist auch der Grund, warum Serien-ECUs nach einem Kaltstart den Leerlauf anheben und spät zünden: Das heisse Abgas heizt den Kat schnell auf (Backlog).

### 2.11 Drehdynamik
  J_eng · dω/dt = T_e − T_load

T_load ist das Moment der Bremse (Dyno) oder des Antriebsstrangs. J_eng (Kurbeltrieb + Schwungrad) ≈ 0,15–0,25 kg·m² für einen 3,0-l-Motor mit Zweimassenschwungrad. **[prüfen]** an Herstellerdaten.

### 2.12 Dyno (Motorprüfstand)
Die Bremse ist ein Regler, der T_load so stellt, dass ω einer Solldrehzahl folgt:
- `holdRpm`: PID-Regler auf N_soll.
- `sweep`: Rampe N_soll(t) = N_start + r·t, z. B. r = 300–600 1/min pro Sekunde. Langsam genug, dass der Motor quasi-stationär ist.

Gemessen wird T_load (bei konstantem ω gilt T_e = T_load). Bei Rampen korrigiert man um J_eng · dω/dt. Leistung P = T · ω, Ausgabe in kW und PS. Messwerte leicht tiefpassfiltern (erster Ordnung, τ ≈ 50 ms).

---

## 3. ECU-Konzepte

### 3.1 Lastberechnung
| Verfahren | Last-Achse | Typisch für |
|---|---|---|
| Speed Density | MAP (Saugrohrdruck) | Speeduino, rusEFI, viele Freely-Programmable ECUs |
| Alpha-N | Drosselklappenwinkel | Einzeldrosselklappen, Rennmotoren |
| MAF | gemessene Luftmasse | Serien-ECUs (Bosch ME/MED) |

tuning-sim startet mit Speed Density, weil die VE-Tabelle dort direkt der Motorphysik (§2.2) entspricht.

### 3.2 Haupttabellen
- **VE** (N × MAP, %): Kraftstoffgrundmenge.
- **Lambda-/AFR-Ziel** (N × MAP): λ ≈ 1,0 Teillast, 0,85–0,90 Volllast Sauger, 0,75–0,82 Volllast Turbo (Bauteilschutz durch Kühlung).
- **Zündung** (N × MAP, °v. OT): Teillast früh (30–40°), Volllast später, unter Boost noch später.
- **Boost-Ziel** (N × Pedal, kPa) und **Wastegate-Duty** (Open Loop).
- Korrekturen: Ansauglufttemperatur, Kühlmitteltemperatur, Beschleunigungsanreicherung.

In echten ECUs (Speeduino) sind VE und Zündung typischerweise 16 × 16 Tabellen. Werte unter Ladedruck dürfen > 100 % sein.

### 3.3 Regelkreise
- **Lambda-Regelung (Closed Loop):** PI-Korrektur der Einspritzmenge nach der Lambdasonde, nur in der Teillast. Die Sonde hat Verzögerung (Transportzeit + Sensor, ≈ 50–200 ms), modellieren als Totzeit + PT1.
- **Boost-Regelung:** Open Loop (Duty-Tabelle) + PID auf das Boost-Ziel. Ein falsch eingestellter PID schwingt, genau das soll man sehen können.
- **Klopfregelung:** bei Klopfen Zündung pro Ereignis um z. B. 1,5–3° zurücknehmen, danach langsam wieder heranführen.
- **Drehzahlbegrenzer:** harte Kraftstoffabschaltung oder Zündunterbrechung (klingt anders: Zündunterbrechung schickt unverbranntes Gemisch in den Auspuff → Knallen).
- **Pops & Bangs (Overrun-Map):** statt Schubabschaltung im Schub weiter einspritzen, die Zündung stark zurücknehmen (25–40° gegenüber der Kennfeldzündung) und eventuell anfetten. Die späte Verbrennung schickt zündfähiges Gemisch in den heissen Auspuff (§2.10a), wo es knallt. Kostet Kraftstoff, heizt Kat und Krümmer stark auf. In tuning-sim drei Tune-Einstellungen: Schubabschaltung ein/aus, Schub-Spätzündung (°), Schub-Anfettung (%).
- **Schubabschaltung (DFCO):** Drosselklappe geschlossen und Drehzahl über einer Einschaltschwelle → kein Kraftstoff. Wieder an unter einer Wiedereinsetzschwelle oberhalb des Leerlaufs (sonst stirbt der Motor ab) oder sobald Gas gegeben wird. Typisch: ein ~1500/min, aus ~1100/min. Spart Kraftstoff und bremst; im Sound bleiben nur schwache Pumpstösse.

### 3.4 Tuning-Funktionen (ab Iteration 10)
- **Launch Control:** im Stand Drehzahlbegrenzer auf N_launch (z. B. 4000/min) mit Zündverstellung nach spät; mit Turbo baut sich so Ladedruck auf.
  - **Modell in tuning-sim (Iteration 7c, «2-Step»):** aktiv, wenn eingeschaltet, Launch-Taste gedrückt (später: Kupplung getreten im Stand) und Gas offen. Die ECU zieht die Zündung um θ_launch zurück und hält N_launch mit **Zündunterbrechung** und der Hysterese des Drehzahlbegrenzers; eingespritzt wird weiter. Im MVM heisst ein Schritt mit Zündunterbrechung: im Zylinder verbrennt nichts (T_i = 0, keine Abgaswärme aus Verbrennung), das zündfähige Gemisch m_ab = min(m_fuel, m_air/AFR_st) geht unverbrannt in den Auspuff und wird wie in §2.10a mit η_ab dem Kat zugerechnet (f_late = 1). Das schnelle Wechseln zwischen Zünden und Unterbrechen (≈ 25 Mal pro Sekunde beim Referenzmotor) ist das typische Stottern; die Knaller kommen aus der Nachverbrennung und brauchen einen heissen Kat.
- **Flat Shift:** Kupplung getreten bei Vollgas → kurze Zündunterbrechung statt Gaswegnehmen.
- **Pops & Bangs (Overrun):** im Schub statt Schubabschaltung etwas Kraftstoff mit sehr später Zündung → Nachverbrennung im heissen Auspuff → Knallen. Im Modell: Ereignisse mit hoher Abgasenthalpie und zufälligen Zündaussetzern.

### 3.5 Formate
- **TunerStudio .ini** («ECU Definition File»): beschreibt Tabellen, Achsen, Skalierungen und Gauges. Spezifikation als PDF von EFI Analytics.
- **.msq**: Tune-Dateien als XML. Ein Import ist ein späteres Ziel (Iteration 13).
- rusEFI und Speeduino sind **GPL**. Konzepte und Formate studieren, Code nicht kopieren.

---

## 4. Klopfen

### 4.1 Phänomen
Klopfen ist die Selbstzündung des Endgases vor der Flammenfront. Es begrenzt Zündwinkel und Ladedruck. Treiber: hoher Druck und hohe Temperatur im Endgas, frühe Zündung, hohe Ansauglufttemperatur, niedrige Oktanzahl, magere Gemische.

### 4.2 Livengood-Wu-Integral
Selbstzündung tritt auf, wenn

  ∫_{t_0}^{t} dt / τ(p, T) ≥ 1

mit der Zündverzugszeit nach Douaud & Eyzat:

  τ = 17,68 ms · (ON/100)^{3,402} · p^{−1,7} · exp(3800 / T)   (p in atm, T in K)   **[prüfen]** SAE 780080

Im 0D-Modell (§7) wird das Integral über den Kurbelwinkel bis zum Brennende ausgewertet. Im MVM nutzt man einen Ersatz:

### 4.3 Vereinfachtes Klopfmodell für das MVM
Klopfindex K als Funktion von Zündwinkel relativ zu einer Klopfgrenze θ_KL(N, p_m, T_m, ON):

  K = (θ_ign − θ_KL) / θ_scale

θ_KL sinkt mit steigendem Saugrohrdruck und steigender Ansauglufttemperatur und steigt mit der Oktanzahl. K > 0 bedeutet Klopfen. Folgen im Modell: Klopfregelung nimmt Zündung zurück; ohne Regelung Momentverlust und Warnung «Motorschaden-Risiko». Parameter so wählen, dass ein Saugmotor bei Serienzündung knapp nicht klopft und ein Turbo bei +3° unter vollem Boost klopft.

---

## 5. Aufladung

### 5.1 Verdichter (Turbo und Zentrifugalkompressor)
Druckverhältnis Π_c = p_2 / p_1. Isentrope Verdichtung mit Wirkungsgrad η_c:

  T_2 = T_1 · ( 1 + (Π_c^{(γ−1)/γ} − 1) / η_c )
  P_c = ṁ_c · c_p · T_1 · (Π_c^{(γ−1)/γ} − 1) / η_c

Kennfeld: Π_c und η_c als Funktion des **korrigierten Massenstroms** und der **korrigierten Drehzahl**:

  ṁ_corr = ṁ · √(T_1/T_ref) / (p_1/p_ref),   N_corr = N_tc / √(T_1/T_ref)

Grenzen im Kennfeld: **Pumpgrenze (Surge)** links (zu wenig Durchfluss bei hohem Π → Strömungsabriss, Rückströmung, «Flattern») und **Stopfgrenze (Choke)** rechts. Für den Start ein analytisch parametrisiertes Kennfeld (z. B. Drehzahllinien als Parabeln, Wirkungsgrad als Ellipse um den Bestpunkt), später digitalisierte Herstellerkennfelder.

### 5.2 Turbine
Leistung:

  P_t = ṁ_t · c_p,exh · T_3 · η_t · ( 1 − Π_t^{−(γ_e−1)/γ_e} ),   Π_t = p_3 / p_4

Durchfluss nach dem Ellipsengesetz (Stodola), als einfacher Ansatz:

  ṁ_t · √T_3 / p_3 = k_t · √(1 − Π_t^{−2})

Wirkungsgrad η_t als Funktion der Schnelllaufzahl (Blade Speed Ratio) u/c_s, Maximum bei ≈ 0,6–0,7.

### 5.3 Turbowelle
Leistungsbilanz:

  J_tc · ω_tc · dω_tc/dt = η_m · P_t − P_c

J_tc ≈ 1·10⁻⁵ bis 1·10⁻⁴ kg·m², Wellendrehzahlen 100 000–250 000 1/min. Das kleine J und die Abhängigkeit vom Abgasstrom erklären das **Turboloch**. Numerik: bei sehr kleinem ω_tc durch null teilen vermeiden (ω_min klemmen oder Formulierung über kinetische Energie E = ½ J ω²).

### 5.4 Wastegate, Ladeluftkühler, BOV
- **Wastegate:** Blende parallel zur Turbine (Formel §2.5 mit Abgaswerten). Öffnung über Dose (Federvorspannung gegen Ladedruck) plus Magnetventil-Duty aus der ECU. Abgas durch das Wastegate erzeugt keine Turbinenleistung.
- **Ladeluftkühler:** T_out = T_in − ε_ic · (T_in − T_amb), ε_ic ≈ 0,6–0,8; Druckverlust Δp ≈ k · ṁ².
- **Schubumluftventil / BOV:** Wird die Drosselklappe bei Ladedruck geschlossen, öffnet das Ventil (Unterdruck im Saugrohr gegen Ladedruck) und bläst ab: ins Freie (BOV, laut) oder zurück vor den Verdichter (Umluft, leise). Ohne Ventil läuft der Verdichter in die Pumpgrenze → Verdichterpumpen («Flattern», «Stutu»).
- **Ladestrecke:** Behältermodell wie §2.5 zwischen Verdichter und Drosselklappe (Volumen inkl. Ladeluftkühler, typisch 3–8 l).

### 5.5 Mechanischer Lader (Supercharger)
Antrieb über Riemen: N_sc = i_belt · N.
- **Verdränger (Roots, Schraube/Twin-Screw):** fördert pro Umdrehung ein festes Volumen V_sc.
  ṁ_sc = η_v,sc · ρ_1 · V_sc · N_sc / 60
  Ladedruck entsteht, weil der Lader mehr fördert, als der Motor schluckt. Druck sofort ab niedriger Drehzahl, kein Turboloch.
  Antriebsleistung: P_sc = ṁ_sc · c_p · T_1 · (Π^{(γ−1)/γ} − 1) / η_sc (Roots: η_sc niedrig, ≈ 0,5–0,6, weil ohne innere Verdichtung; Schraube ≈ 0,65–0,75).
- **Zentrifugal:** Verdichterkennfeld wie §5.1, aber Drehzahl fest an den Motor gekoppelt → Ladedruck steigt etwa quadratisch mit der Drehzahl.
- Das Antriebsmoment T_aux = P_sc / ω wird vom Motormoment abgezogen (§2.9). Bypass-Ventil öffnet in der Teillast und senkt die Antriebsleistung.

---

## 6. Akustik: Motorsound

### 6.1 Was den Klang ausmacht
1. **Quelle:** Beim Öffnen des Auslassventils (EVO) strömt Abgas unter hohem Druck (3–6 bar bei Volllast) schlagartig aus. Das ist ein kurzer Druck- und Massenstromimpuls, die Hauptschallquelle.
2. **Zeitmuster:** Zündfolge und Zündabstände bestimmen, wann welche Impulse in welches Rohr laufen (§1.3).
3. **Übertragung:** Krümmerrohre, Sammler, Katalysator, Schalldämpfer, Endrohr wirken als akustisches Leitungssystem mit Laufzeiten, Reflexionen und Resonanzen.
4. **Abstrahlung:** Mündung des Endrohrs und Raum (Prüfstandshalle).
5. Daneben: Ansauggeräusch, Mechanik (Ventiltrieb, Kette), Lader, Getriebe.

Ein Mean-Value-Modell hat Punkt 1 und 2 nicht. Daher braucht der Sound ein **ereignisbasiertes, kurbelwinkelaufgelöstes** Modell.

### 6.2 Stufe A (Iteration 3/7): hybrides Impulsmodell
Im Audio-Thread mit f_s = 48 kHz:
1. Kurbelwinkel integrieren: θ += ω · (1/f_s) mit ω aus der Simulation (zwischen Updates linear interpoliert).
2. Für jeden Zylinder k prüfen, ob θ den Winkel θ_EVO,k (= Zündwinkel des Zylinders + ca. 130–140°) überschritten hat. Dann einen Impuls auslösen.
3. **Impulsamplitude** A_k aus dem MVM: proportional zum Zylinderdruck bei EVO, näherungsweise ∝ Luftmasse pro Zylinder × Faktor, der bei Spätzündung steigt (heisseres, druckreicheres Abgas) und bei Zündaussetzer/Schub sinkt. Kleine Zufallsschwankung (±3–8 %) und Zeitjitter (≈ ±0,5° KW) für Natürlichkeit (zyklische Verbrennungsschwankungen).
4. **Impulsform:** kurz und **bandbegrenzt**, damit keine Aliasing-Artefakte entstehen. Varianten: Hann-Fenster-Puls der Dauer ≈ 1,5–3 ms (skaliert mit Drehzahl), gedämpfte Sinusschwingung, oder ein vorberechneter, gefilterter Impuls aus einer Tabelle (Wavetable). Harte Rechteckimpulse vermeiden.
5. Impuls in die Verzögerungsleitung des zugehörigen Krümmerrohrs einspeisen (§6.3).

**Umsetzung (Iteration 7, ADR 0007):** Amplitude ∝ Füllung (Luftmasse pro Arbeitsspiel relativ zu VE = 1 bei Umgebung), +1 %/° Spätzündung. Spätzündung macht die Pulse zudem weicher (länger, flacherer Anstieg) und senkt den Tiefpass: Die späte, langsamere Verbrennung lässt den Druck bei EVO weniger abrupt ansteigen. Nur den Tiefpass zu senken, reicht nicht, weil die hörbare Energie in den Harmonischen unter ~1,5 kHz liegt. λ > 1,05 erhöht die Zyklusschwankung (rauer). Bei Kraftstoffabschaltung bleiben schwache, dumpfe Pumpstösse.

### 6.3 Auspuff als digitaler Wellenleiter
Schall breitet sich in einem Rohr als Welle mit Schallgeschwindigkeit c aus:

  c = √(γ · R · T)   heisses Abgas bei 900 K: c ≈ √(1,33 · 287 · 900) ≈ 590 m/s

Ein Rohr der Länge L ist eine Verzögerung um L/c Sekunden, also **D = L · f_s / c Samples** (nicht ganzzahlig → fraktionale Verzögerung, z. B. lineare oder Allpass-Interpolation). Ein **digitaler Wellenleiter** besteht aus zwei Verzögerungsleitungen (hin und zurück).

An Querschnittsänderungen wird reflektiert. Mit der akustischen Impedanz Z = ρc/A gilt für den Reflexionsfaktor einer Welle von Rohr 1 nach Rohr 2:

  r = (Z₂ − Z₁) / (Z₂ + Z₁) = (A₁ − A₂) / (A₁ + A₂)

- Offenes Ende (Endrohr ins Freie): r ≈ −1, frequenzabhängig (hohe Frequenzen strahlen besser ab). Modell: −1 mal Tiefpass, der durchgelassene Anteil ist der abgestrahlte Schall.
- Geschlossenes Ende: r ≈ +1.
- Verzweigung/Sammler (mehrere Rohre in einen Knoten): Streuknoten (Scattering Junction). Druck im Knoten p_J = 2 · Σ(Γ_i · p_i⁺) / Σ Γ_i mit Admittanz Γ_i = A_i/(ρc); ausgehende Welle je Rohr p_i⁻ = p_J − p_i⁺.
- Verluste (Wandreibung, Wärme): pro Durchlauf ein leichter Tiefpass und Faktor < 1.

Struktur für einen I6 mit 6-in-1-Krümmer: 6 Krümmer-Wellenleiter → Sammlerknoten → Mittelrohr → (Schalldämpfer) → Endrohr → Abstrahlung. Für 3-in-1 ×2 (typisch BMW) zwei Sammler und ein Y-Stück. Unterschiedliche Rohrlängen ergeben unterschiedliche Laufzeiten, so entsteht z. B. der Boxer-Klang durch ungleich lange Krümmer.

Quelle: J. O. Smith, *Physical Audio Signal Processing*; Baldan et al. 2015.

### 6.4 Schalldämpfer und Raum per Faltung
Ein Schalldämpfer mit Kammern ist als Wellenleiter aufwendig. Pragmatisch: **Faltung** des Endrohrsignals mit einer Impulsantwort (IR):

  y[n] = Σ_k h[k] · x[n − k]

Für lange IRs (> 256 Samples) **partitionierte FFT-Faltung** (Uniformly Partitioned Overlap-Save) verwenden, damit die Latenz klein bleibt. IRs als WAV in `shared/impulses/`, zuerst synthetisch (abklingendes, gefiltertes Rauschen), später gemessen oder selbst entworfen. Eine zweite, kurze IR für den Raum (Prüfstandshalle).

### 6.5 Ansaug- und Mechanikgeräusch
- **Ansaugen:** Unterdruckimpulse bei IVO, gefiltert (Luftfilterkasten als Helmholtz-Resonator, f_H = (c/2π) · √(A_Hals / (V · L_Hals))), leiser als der Auspuff, aber bei Vollgas und offener Drosselklappe deutlich (Ansaugröhren).
- **Mechanik:** Rauschen mit Ordnungsstruktur (Ventiltrieb: Ordnungen der Nockenwelle = halbe Motordrehzahl), sehr leise beimischen.

### 6.6 Lader-Geräusche
- **Turbopfeifen:** Die Schaufelpassierfrequenz f_BPF = (N_tc/60) · z_Schaufeln liegt bei 150 000/min und 6+6 Schaufeln bei 15–30 kHz, also teils über dem Hörbereich. Das typisch hörbare «Spool»-Pfeifen besteht aus Subharmonischen, Drehklang der Welle und gefiltertem Strömungsrauschen im Bereich ≈ 2–8 kHz. Modell: ein oder mehrere Sinustöne mit f ∝ N_tc (in den Hörbereich skaliert, mit `pitchScale` einstellbar), Amplitude ∝ Massenstrom und Π_c, plus bandpassgefiltertes Rauschen. Ehrlich dokumentieren, dass hier eine künstlerische Skalierung im Spiel ist.
- **BOV:** Abblasimpuls aus gefiltertem Rauschen (Bandpass ≈ 1–5 kHz) mit schnellem Anstieg und Abklingzeit 0,2–0,6 s; Amplitude und Dauer ∝ Ladedruck beim Öffnen.
- **Verdichterpumpen (ohne BOV):** periodische Rückströmung mit wenigen bis ≈ 20 Hz, als amplitudenmodulierter Rauschburst («Flattern») mit sinkender Frequenz.
- **Kompressorheulen:** Verdränger mit z Flügeln pro Rotor und zwei Rotoren erzeugen etwa 2z Förderpulse pro Laderumdrehung: f ≈ (N_sc/60) · 2z (plus Harmonische). Bei N_sc = 12 000/min und 3 Flügeln: 200 · 6 = 1200 Hz. Klang: Sägezahn-artiger Ton, bandbegrenzt synthetisiert, Amplitude ∝ Druckverhältnis.
- **Turbo dämpft den Auspuff:** Die Turbine entzieht dem Abgas Energie und glättet die Pulse. Im Akustikmodell: Pulse hinter der Turbine mit Faktor < 1 und zusätzlichem Tiefpass.

### 6.7 Fehlzündungen und Pops
Unverbranntes Gemisch, das im Auspuff nachverbrennt: zusätzliche, kräftige, breitbandige Impulse (kurzer Knall), zufällig zeitversetzt nach Zündunterbrechung oder Schub mit Overrun-Map. Wahrscheinlichkeit ∝ Kraftstoff im Auspuff und Abgastemperatur.

### 6.8 Echtzeit-Regeln für Audio
- Puffer 256–512 Frames bei 48 kHz (5–10 ms Latenz).
- Im Callback: keine Allokation, keine Locks, kein Logging, keine Exceptions.
- Parameter aus der Simulation glätten (lineare Rampen pro Puffer), sonst «Zipper-Rauschen».
- DC-Anteil entfernen (Hochpass ≈ 20 Hz), Ausgang begrenzen (Soft-Clipper statt hartem Clipping).
- Alles bandbegrenzt erzeugen; bei hohen Drehzahlen werden Impulse kürzer → Aliasing-Gefahr, deshalb Impulslänge nach unten begrenzen oder Oversampling.

### 6.9 Wie engine-sim es macht (Referenz)
engine-sim (Ange Yaghi, MIT-Lizenz) simuliert Kolben, Pleuel und Kurbelwelle als Starrkörper mit Constraint-Solver und ein Gassystem für Brennraum, Ein- und Auslass. Die Physik läuft standardmässig mit 10 kHz (in Motorskripten bis 40 kHz), pro Abgasanlage wird ein Kanal in einen Synthesizer geschrieben, die Ausgabe erfolgt mit 44,1 kHz. Der Synthesizer verwendet u. a. Faltung mit Impulsantworten, Verzögerung, Feedback-Kammfilter, Jitter und Tiefpass, in einem eigenen Audio-Thread mit ≈ 0,1 s Puffer. Faustregel aus Forks mit 1D-Auspuff: Simulationsschritt ≤ ≈ 2° KW bei Maximaldrehzahl (für 7000/min ≈ 21 kHz). engine-sim ist ausdrücklich kein wissenschaftliches Werkzeug, sondern auf Klang optimiert. Für tuning-sim ist es **Hörreferenz und Lehrbuch**. Eine Einbindung als Adapter (über einen portablen Fork wie open-engine-sim) wäre nach Rückfrage möglich.

---

## 7. Zylinderaufgelöstes 0D-Modell (Iteration 11)

Pro Zylinder wird über den Kurbelwinkel gerechnet (Schritt ≤ 1–2° KW).

### 7.1 Erster Hauptsatz für den Brennraum
  dU/dt = dQ_comb/dt − dQ_wall/dt − p · dV/dt + Σ ṁ_in · h_in − Σ ṁ_out · h

Mit idealem Gas folgt für den Druck (geschlossene Phase, ohne Massenströme):

  dp/dθ = (γ − 1)/V · ( dQ_comb/dθ − dQ_wall/dθ ) − γ · p / V · dV/dθ

V(θ) und dV/dθ aus §1.4.

### 7.2 Brennverlauf nach Wiebe
Verbrannter Massenanteil:

  x_b(θ) = 1 − exp( −a · ((θ − θ_0)/Δθ_b)^{m+1} ),   für θ_0 ≤ θ ≤ θ_0 + Δθ_b

Typisch a = 5 (99,3 % verbrannt), m = 2, Brenndauer Δθ_b ≈ 40–70 °KW. Wärmefreisetzung dQ_comb/dθ = m_fuel · H_u · η_comb · dx_b/dθ. Der Zündwinkel setzt θ_0 (plus Zündverzug).

### 7.3 Wandwärme nach Woschni (vereinfacht)
  dQ_wall/dt = h_c · A_wall · (T − T_wall),   h_c = 3,26 · B^{−0,2} · p^{0,8} · T^{−0,55} · w^{0,8}

(p in kPa, B in m, w charakteristische Gasgeschwindigkeit ≈ C₁ · v̄_p). **[prüfen]** Heywood Kap. 12.

### 7.4 Ventilströmung
Massenstrom durch Ein- und Auslassventil mit der Blendengleichung (§2.5), Fläche aus Ventilhubkurve L_v(θ) (z. B. Sinus- oder Polynomprofil zwischen Öffnen und Schliessen), A_v = π · d_v · L_v (begrenzt durch den Sitzquerschnitt), Durchflusskoeffizient C_d ≈ 0,6–0,7.

### 7.5 Kopplung an die Akustik
Der Auslassmassenstrom ṁ_out,k(t) jedes Zylinders ersetzt den synthetischen Impuls aus §6.2 als Quellsignal des Wellenleiters (ggf. Schallschnelle/Druck daraus ableiten). Damit hängt der Klang automatisch von Last, Zündung, Steuerzeiten und Drehzahl ab. Das Drehmoment ergibt sich aus ∮ p dV und kann mit dem MVM verglichen werden.

### 7.6 1D-Gasdynamik (optional)
Für Krümmerabstimmung (Resonanzaufladung) die eindimensionalen Euler-Gleichungen in Rohren, z. B. mit einem Finite-Volumen-Verfahren (Lax-Wendroff, MacCormack oder HLL). Aufwendig, deutlich teurer als Wellenleiter, nur bei Bedarf.

---

## 8. Antriebsstrang und Fahrzeug (Iteration 10)

### 8.1 Übersetzungen
  ω_wheel = ω_eng / (i_g · i_f)
  T_wheel = T_clutch · i_g · i_f · η_dl     (η_dl ≈ 0,85–0,92 inkl. Differenzial)

Typisches 6-Gang-Getriebe (Beispiel, Annahme): 4,11 / 2,32 / 1,54 / 1,18 / 1,00 / 0,85, Achse i_f ≈ 3,15.

### 8.2 Fahrzeuglängsdynamik
  m_eff · dv/dt = T_wheel / r_dyn − F_air − F_roll − F_grade
  F_air = ½ · ρ_air · c_w · A · v²,   F_roll = c_r · m · g

m_eff = m + J_wheels / r² + (J_eng · (i_g i_f)²) / r² (rotierende Massen, im gekuppelten Zustand). Typisch: c_w · A ≈ 0,6–0,7 m², c_r ≈ 0,010–0,015.

### 8.3 Kupplung (Zustandsautomat)
- **Haftend:** Motor und Getriebeeingang drehen gleich; ein gemeinsames Trägheitssystem. Übertragenes Moment = was nötig ist, solange |T| ≤ T_haft,max.
- **Gleitend:** T_clutch = μ · F_n · r_m · z_Flächen · sign(ω_eng − ω_in) · Betätigungsgrad. Beide Seiten getrennt integrieren.
- **Übergang gleitend → haftend**, wenn |ω_eng − ω_in| < ε und das nötige Moment ≤ T_haft,max. Diese Logik verhindert numerisches Zappeln (Stick-Slip mit kleinen Zeitschritten testen).

### 8.4 Rollenprüfstand
Statt Strasse treiben die Räder eine Rolle mit Trägheit J_roll. Ein Trägheits-Rollenprüfstand misst Leistung aus der Beschleunigung der Rolle: P_wheel = J_roll · ω_roll · dω_roll/dt (plus Verluste). Radleistung ist kleiner als Motorleistung (Triebstrangverluste). Genau dieser Unterschied ist ein schönes Lernthema.

### 8.5 Umsetzung in tuning-sim (Iteration 10)
- **Rolle mit Fahrwiderstand** («road-load»): Die Rolle bildet Masse und Fahrwiderstand des Autos nach, das Auto fährt darauf wie auf der Strasse (§8.2): m_v = m + J_Räder / r², F_res = ½ ρ c_w A v² + c_r m g.
- **Kupplung als Zustandsautomat** (§8.3): offen (Leerlauf, Pedal, Schaltpause) → gleitend (T_c = ±e · T_max mit Einkupplungsgrad e) → haftend. Haftend wird gemeinsam integriert:
  dv/dt = (T_e i η / r − F_res) / (m_v + J_e i² η / r²),  T_c = T_e − J_e (i/r) dv/dt.
  Übersteigt |T_c| die Kapazität, rutscht sie wieder. Gleitend → haftend, wenn der Schlupf kleiner als 1 rad/s ist oder im Schritt das Vorzeichen wechselt; die gemeinsame Drehzahl erhält den Drehimpuls (J_e ω_e + J_in ω_in)/(J_e + J_in) mit J_in = m_v r² / i².
- **Ein Schritt** wird um den Motor geteilt: Motormoment bei aktueller Drehzahl → Kupplungsmoment als Last → Kurbel integrieren → Auto integrieren (haftend kinematisch v = ω r / i, kein Auseinanderdriften).
- **Schalten** sequenziell mit automatischer Kupplung: 150 ms Zugkraftunterbrechung (Kupplung offen, Gang wechselt am Ende), danach schliesst die Kupplung in 150 ms. **Flat Shift:** In der Unterbrechung bei offenem Gas unterbricht die ECU die Zündung (wie der 2-Step), sonst dreht der Motor hoch.
- **Launch Control** ist scharf, wenn die Kupplung im Stand (< 5 km/h) getreten ist; Kupplung loslassen = Losfahren mit gleitender Kupplung.
- **Bremse (10b):** Volle Bremse verzögert das fahrende Auto mit 0,9 g (Reifenhaftung): F_B = b · m · a_max. Im Stand hält sie das Auto gegen den Antrieb bis zum 2,5-fachen davon (das Bremsmoment wirkt direkt am angetriebenen Rad, nicht über die Haftung), zieht aber nie rückwärts. Rastet die Kupplung bei gebremstem, stehendem Auto ein, nimmt die Bremse den Restimpuls auf: der Motor stirbt ab. Gemessen: 60 → 0 km/h in etwa 1,9 s.
- **Abwürgen:** Fällt die Drehzahl unter 350/min, ist der Motor aus; sobald der Antrieb getrennt ist, bringt ihn der «Anlasser» auf Leerlauf.
- **Rollenlauf:** Vollgas in einem Gang ab 300/min unter der ersten Messdrehzahl. Die Rolle misst die Antriebskraft F an den Reifen, Radleistung P = F v; Radmoment / i als motorbezogenes Moment. Ergebnis beim Referenzauto im 4. Gang: 175 kW an den Rädern gegenüber 196 kW am Motor (−11 %: η = 0,90 plus die Leistung, die die Motorträgheit beschleunigt).

---

## 9. Numerik

- **Fester Zeitschritt**, entkoppelt von der Bildrate (Akkumulator-Muster, Glenn Fiedler «Fix Your Timestep»).
- **Explizites Euler:** x_{n+1} = x_n + Δt · f(x_n). Einfach, aber nur stabil, wenn Δt deutlich kleiner als die kleinste Zeitkonstante. Kritische Kandidaten: Saugrohrdruck (τ = V_m / (η_v V_d N/120 · ...) wenige ms), Turbowelle bei kleinem ω, Kupplung.
- **Semi-implizites Euler** (erst Geschwindigkeit, dann Position mit neuer Geschwindigkeit) für Drehdynamik, energetisch gutmütiger.
- **RK4** für das 0D-Zylindermodell oder Teilschritte (Substepping) für steife Teilsysteme.
- Stabilitätstest: Simulation mit Δt und Δt/2 vergleichen; Ergebnisse dürfen sich kaum unterscheiden.
- Tiefpass erster Ordnung (diskret): y_n = y_{n−1} + α · (x_n − y_{n−1}), α = Δt / (τ + Δt).
- PID diskret mit Anti-Windup (Integrator klemmen) und Ableitung auf den Messwert (nicht auf den Fehler), damit Sollwertsprünge nicht «kicken».

---

## 10. Plausibilitätswerte für Tests

| Grösse | Plausibler Bereich |
|---|---|
| VE Sauger Volllast | 0,85–1,05 |
| bmep Sauger | 11–13,5 bar |
| bmep Turbo | 18–25 bar |
| Spitzenmoment 3,0-l-I6 Sauger | ≈ 280–330 Nm |
| Spitzenleistung 3,0-l-I6 Sauger | ≈ 190–230 kW |
| Leerlauf MAP | 30–45 kPa |
| EGT Volllast | 700–950 °C |
| Mittlere Kolbengeschwindigkeit bei Nenndrehzahl | 15–22 m/s |
| Turbowellendrehzahl | 100 000–250 000 1/min |
| Zündfrequenz I6 bei 6000/min | 300 Hz |
| Schallgeschwindigkeit Abgas | ≈ 550–650 m/s |

---

## 11. Quellen

**Bücher**
- J. B. Heywood: *Internal Combustion Engine Fundamentals*, 2. Aufl., McGraw-Hill, 2018. Grundlagen, Kinematik, Ladungswechsel, Verbrennung, Wärmeübergang, Reibung.
- L. Guzzella, C. H. Onder: *Introduction to Modeling and Control of Internal Combustion Engine Systems*, 2. Aufl., Springer, 2010. Mean-Value-Modelle, Saugrohr, Drehmomentmodell, Turbo, Regelung. **Hauptquelle für §2 und §5.**
- J. O. Smith III: *Physical Audio Signal Processing*, online (ccrma.stanford.edu/~jos/pasp/). Digitale Wellenleiter, Streuknoten, fraktionale Verzögerung.
- Bosch: *Kraftfahrtechnisches Taschenbuch*. Kennwerte, Fahrzeugdynamik.

**Artikel**
- J. Müller, E. Hendricks, S. C. Sorenson: «Mean Value Modelling of Turbocharged Spark Ignition Engines», SAE 980784, 1998.
- E. Hendricks, S. C. Sorenson: «Mean Value Modelling of Spark Ignition Engines», SAE 900616, 1990.
- A. M. Douaud, P. Eyzat: «Four-Octane-Number Method for Predicting the Anti-Knock Behavior of Fuels and Engines», SAE 780080, 1978.
- J. C. Livengood, P. C. Wu: «Correlation of Autoignition Phenomena in Internal Combustion Engines and Rapid Compression Machines», Fifth Symposium on Combustion, 1955.
- S. K. Chen, P. F. Flynn: «Development of a Single Cylinder Compression Ignition Research Engine», SAE 650733, 1965 (Reibungskorrelation).
- G. Woschni: «A Universally Applicable Equation for the Instantaneous Heat Transfer Coefficient in the Internal Combustion Engine», SAE 670931, 1967.
- S. Baldan, H. Lachambre, S. Delle Monache, P. Boussard: «Physically informed car engine sound synthesis for virtual and augmented environments», IEEE SIVE, 2015.
- G. Fiedler: «Fix Your Timestep!», gafferongames.com.

**Open-Source-Referenzen** (Lizenz vor Übernahme prüfen)
- engine-sim, Ange Yaghi – github.com/ange-yaghi/engine-sim (MIT)
- open-engine-sim – github.com/carlesonielfa/open-engine-sim (portabler Fork)
- engine-sim-tuner – github.com/tierlord/engine-sim-tuner (Headless-Server + Tuning-Cockpit)
- Engine_Sim (Browser, Wellenleiter + AudioWorklet) – github.com/YigitSalihEmecen/Engine_Sim
- rusEFI – github.com/rusefi/rusefi, wiki.rusefi.com (GPL, nur Konzepte)
- Speeduino – github.com/noisymime/speeduino, speeduino.com/wiki (GPL, nur Konzepte)
- TunerStudio ECU Definition Files – efianalytics.com (Format-Spezifikation)
- miniaudio – github.com/mackron/miniaudio
