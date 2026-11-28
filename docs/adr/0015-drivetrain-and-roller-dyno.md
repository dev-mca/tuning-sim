# ADR 0015 – Antriebsstrang und Rollenprüfstand

- Status: angenommen
- Datum: 2026-11-28

## Kontext

Iteration 10 bringt Getriebe, Kupplung, Fahrzeug, Rollenprüfstand, Launch Control mit echtem
Losfahren und Flat Shift. Der frei drehende Motor und der Motorprüfstand (Sweep) müssen dabei
genau so weiterlaufen wie bisher, die bestehenden Golden Files dürfen sich nicht ändern.

## Entscheide

1. **Das Auto steht immer auf einem Rollenprüfstand mit Fahrwiderstand** (research.md §8.5).
   Im Leerlauf oder mit getretener Kupplung ist der Motor frei wie vorher; eine eigene
   «Fahrmodus»-Umschaltung entfällt. Verworfen: getrennte Modi «Motor frei» und «Fahren»
   (mehr Zustände, mehr Protokoll).
2. **Fahrzeugdaten als Block `drivetrain` in der Motordatei.** Es gibt genau ein Auto; eine
   eigene Datei `shared/vehicles/` lohnt sich erst mit mehreren Autos (Backlog).
3. **Schritt um den Motor geteilt** (`prepare` → Kupplungsmoment → `finish`), statt das
   Lastmoment einen Schritt verzögert zu nehmen. So ist die haftende Kupplung exakt (keine
   Ein-Schritt-Verzögerung, kein Stick-Slip-Flattern).
4. **Kupplung als Zustandsautomat** offen/gleitend/haftend mit Drehimpulserhaltung beim
   Einrasten und kinematischer Kopplung im haftenden Zustand.
5. **Sequenzielles Schalten mit automatischer Kupplung** (`shift`), dazu ein echtes
   Kupplungspedal (`setClutch`) für das Losfahren. **Flat Shift** ist eine Einstellung im
   Launch-Block (`launchSettings.flatShift`), weil beide Tuning-Funktionen dieselbe
   Zündunterbrechung nutzen; kein eigener Nachrichtentyp.
6. **Rollenlauf als eigener Lauf** (`startRollerRun`, `dynoMode: roller`), Ergebnis als
   `dynoResult` mit `kind: "roller"`, damit er im selben Overlay wie der Motorlauf erscheint.
   `torque_Nm` ist dort Radmoment / Gesamtübersetzung, der Unterschied zum Motorlauf zeigt die
   Verluste.
7. **Engine-Dyno-Sweep nimmt den Gang heraus**; während eines Laufs wird Schalten abgelehnt.

## Folgen

- Neue Golden Files `i6_30_na_roller.csv` und `i6_30_na_drive.csv`; die alten bleiben bitgleich.
- Der Rollenlauf dauert in Echtzeit lange (4. Gang ~37 s), wie auf einem echten Prüfstand mit
  Fahrwiderstand.
- Turbo (Iteration 8) kann den 2-Step im Stand für Ladedruckaufbau nutzen; Launch über die
  Kupplung ist dafür schon da.
