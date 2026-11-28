# ADR 0014 – Launch Control (2-Step) im Stand (vorgezogen)

- Status: angenommen
- Datum: 2026-11-28

## Kontext

Launch Control gehört laut plan.md zu Iteration 10 (Antriebsstrang). Der Nutzer wollte den
2-Step sofort hören. Ohne Getriebe gibt es kein «Stehen» und keine Kupplung, also braucht es
einen Ersatz für den Schalter, der den 2-Step scharf macht.

## Entscheide

1. **Eigene Iteration 7c**, nur ECU, MVM, Sound, Protokoll und UI; Getriebe bleibt in 10.
2. **Launch-Taste als Live-Eingang** (`setLaunchSwitch`, Taste L), nicht Teil des Tunes. Sie
   steht für «Kupplung getreten im Stand» und wird in Iteration 10 durch Kupplung und
   Fahrzeuggeschwindigkeit ersetzt bzw. ergänzt. Nach jedem `hello` ist sie losgelassen
   (sicherer Zustand), während eines Dyno-Sweeps ist sie wirkungslos.
3. **Zündunterbrechung mit Einspritzung** statt Kraftstoffabschaltung, wie bei echten 2-Steps.
   Im MVM als binärer Zustand je 1-ms-Schritt mit der Hysterese des Drehzahlbegrenzers
   (research.md §3.4). Vorteil: keine neue Motorparameter, das Stottern entsteht von selbst.
   Verworfen: Anteil unterbrochener Zyklen als stetiger Wert (glatter, aber das typische
   Stottern müsste die Akustik künstlich erzeugen).
4. **Knaller über die bestehende Nachverbrennung** (ADR 0008): Bei Zündunterbrechung gilt
   f_late = 1, `afterburnRate` wird auch bei aktivem 2-Step gesetzt, der Kat muss heiss sein.
5. **Grenzen:** Launch-Drehzahl 2000–6500/min, Spätzündung 0–30°. Serie: aus.
6. **Golden File `i6_30_na_launch.csv`**; die bestehenden Golden Files bleiben bitgleich, weil
   ohne Zündunterbrechung jede Formel in derselben Reihenfolge rechnet.

## Folgen

- `EcuCommand`, `OperatingPoint` und `StateSnapshot` haben neue Felder (Lab und C++).
- Mit Turbo (Iteration 8) baut der 2-Step im Stand Ladedruck auf; dafür reicht die Spätzündung
  hier als Grundlage.
