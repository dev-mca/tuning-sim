# ADR 0008 – Pops & Bangs im Schub (vorgezogen)

- Status: angenommen
- Datum: 2026-11-22

## Kontext

plan.md sieht «Pops & Bangs im Schub» und Fehlzündungen erst in Iteration 10 vor. Der Nutzer
wollte sie nach Iteration 7 sofort: Schubabschaltung abschalten, ECU-Remap mit Wirkung auf das
Blubbern und Knallen, und laut seinem Backlog ein Kat, der warm sein muss. Deshalb gibt es eine
eingeschobene Iteration 7b. Launch Control und Flat Shift bleiben in Iteration 10.

## Entscheide

1. **Schub-Einstellungen als Teil des Tunes** (`OverrunSettings` im `Tune`, Lab und C++):
   Schubabschaltung ein/aus, Schub-Spätzündung (° gegenüber dem Kennfeld) und Schub-Anfettung.
   Drei Skalare statt einer eigenen Tabelle. Das reicht zum Lernen und passt in ein kleines Panel.
   Eine `overrunPops`-Tabelle über der Drehzahl bleibt für Iteration 10 offen.
2. **Physik zuerst im Lab, dann C++ identisch** (research.md §2.10a): Anteil später Verbrennung
   bei grossem Abstand zu MBT (ab 25°, damit die Leerlauf-Momentenreserve nicht zählt),
   Nachverbrennungswärme und eine **Katalysatortemperatur** als Glied erster Ordnung (τ = 10 s).
   Neues Golden File `i6_30_na_overrun.csv` (6 s Gasstoss und Schub mit Pops-Tune); der C++-Port
   stimmt bitgenau. Moment und Abgastemperatur ändern sich nicht, das Vollgas-Golden-File bleibt gleich.
3. **Knaller im Sound** (`acoustic/afterburn.*`): Jedes Öffnen eines Auslassventils kann mit der
   Wahrscheinlichkeit «ECU spritzt im Schub ein × später Anteil × Kat-Bereitschaft (480–630 °C) ×
   0,07» einen Knall auslösen. Ein Knall besteht aus einem Rausch-Crack und einem tiefen «Wumm»,
   meist leises Knistern, selten ein grosser Bang. Die Knaller umgehen den Klang-Tiefpass, sonst
   wären sie auf dem dumpfen Schub nur ein Plopp.
4. **Protokoll**: `getOverrunSettings` / `setOverrunSettings` (Anfettung in %, Bereichsprüfung,
   Broadcast `overrunSettings`), `state.catalyst_C` und `state.overrunFueling`. Die Einstellungen
   stehen in der Tune-Datei (optional, alte Dateien bleiben gültig) und werden beim «Revert»
   zurückgesetzt. Sie gehören nicht zur Undo-Historie der Tabellen.
5. **Hörprobe ohne UI**: `simcore --sound-blips out.wav [--no-overrun-cut] [--overrun-retard 35]
   [--overrun-enrichment 0.1]`.

## Konsequenzen

- (+) Remap mit hörbarer Wirkung: Serie still, Schubabschaltung aus → leises Blubbern, Pops-Tune →
  Knallen, mit heisserem Kat mehr.
- (+) Abnahme gemessen (`sound_acceptance_test.cpp`): Knallfenster und Pegel im Schub.
- (−) Die Knallwahrscheinlichkeit und -form sind abgestimmte Annahmen.
- (−) Die Abgastemperatur des MVM ist im Leerlauf und bei sehr später Zündung zu hoch
  (~1000–1400 °C). Deshalb wird der Kat schon beim Leerlauf heiss. Steht im Backlog. Die Korrektur
  ändert das Golden File.
- (−) Ohne Fahrzeug dreht der Motor frei: Der Schub dauert nur ~3 s. Mit Getriebe und Fahrzeug
  (Iteration 10) kommen die Pops viel besser zur Geltung.
