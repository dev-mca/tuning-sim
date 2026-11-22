# ADR 0007 – Motorsound in simcore

- Status: angenommen
- Datum: 2026-11-22

## Kontext

Iteration 7 koppelt den Sound aus dem Spike (ADR 0003) an die Simulation (plan.md §8). Gefordert:
Klang aus Last, Zündwinkel und Lambda, Drehzahlbegrenzer und Schubabschaltung hörbar, Lautstärke
über die UI. Abnahme: Vollgas-Sweep glaubwürdig, Spätzündung hörbar matter, kein Knacksen bei
Lastwechseln. Die Thread-Regeln stehen in plan.md §2.2 und research.md §6.8.

## Entscheide

1. **Eigene Bibliothek `simcore_acoustic`** (`core/src/acoustic`, `core/src/dsp`): Der Code aus
   dem Spike ist umgezogen, ohne Abhängigkeit zu Simulation oder I/O. Der Spike nutzt dieselbe
   Bibliothek (keine Kopie), seine Tests liegen jetzt in `core/tests/acoustic`.
2. **`AcousticParams` als Schnittstelle Simulation → Sound** (SI): Drehzahl, Füllung relativ zu
   VE = 1 bei Umgebungsbedingungen, Spätzündung gegenüber MBT, gemessenes Lambda, Schubabschaltung.
   Die Simulation (1 kHz) schreibt sie über den Port `IAcousticSink` in einen Triple Buffer. Der
   Audio-Callback liest pro Block den neuesten Wert. `EngineSound` glättet pro Sample, daher
   gibt es kein Zipper-Rauschen.
3. **Abbildung auf den Klang** (`acoustic/acoustic_params.cpp`, research.md §6.2):
   - Amplitude ∝ Füllung, bei Spätzündung +1 %/° (mehr Restdruck bei EVO).
   - **Spätzündung macht weichere Pulse**: länger (bis 1,5×) und mit flacherem Anstieg. Dazu
     kommt ein tieferer Tiefpass. Nur den Tiefpass zu senken, änderte fast nichts, denn die
     hörbare Energie liegt in den Harmonischen unter etwa 1,5 kHz (gemessen: Spektralschwerpunkt
     −1 %). Mit weichen Pulsen sinkt er um 16–20 %, die Lautstärke ändert sich um höchstens 3 dB.
   - Mageres Gemisch (λ > 1,05) erhöht die Zyklusschwankung und klingt dadurch rauer.
   - Schubabschaltung/Begrenzer: nur schwache, dumpfe Pumpstösse.
4. **Schubabschaltung in der ECU** (zuerst Lab, dann C++ identisch): Drosselklappe zu und über
   1500/min → kein Kraftstoff, wieder an unter 1100/min oder beim Gasgeben. Sie fehlte bisher,
   die Abnahme verlangt aber, dass sie hörbar ist. Das Golden File bleibt unverändert (Vollgas).
5. **Abgasgeometrie aus der Motordatei** (`exhaust` in `shared/engines/*.json`): Krümmer,
   Sammlergruppen, Sammlerrohr, Y-Stück, Endrohr. Wird beim Laden validiert.
6. **Lautstärke als Befehl `setAudio {volume, mute}`**: Die UI hält die Einstellung (localStorage)
   und sendet sie nach jedem `hello`. simcore blendet mit 30 ms Rampe. `hello.audio` meldet Gerät
   oder Grund für «kein Ton».
7. **Offline-Rendering** `simcore --sound-sweep out.wav [--ignition-offset -10]`: Simulation und
   Sound laufen im Gleichschritt (1 ms ↔ 48 Frames). Dient zum Anhören und für die Abnahmetests.
   `--no-audio` startet den Server ohne Audiogerät (CI).

## Konsequenzen

- (+) Die Abnahme ist messbar: Klicks (max. Sample-Sprung), Helligkeit (RMS der Ableitung /
  RMS), Lautstärke über den Sweep, alles in `core/tests/sound_acceptance_test.cpp`.
- (+) Audio bleibt in simcore, nie über WebSocket (ADR 0001).
- (−) Die Abbildung Physik → Klang besteht aus abgestimmten Annahmen (Faktoren in
  `acoustic_params.hpp`), sie ist nicht gemessen. Iteration 11 ersetzt die Pulsquelle durch das
  0D-Modell.
- (−) simcore spielt auf dem Standard-Ausgabegerät des Systems. Eine Geräteauswahl steht im Backlog.
