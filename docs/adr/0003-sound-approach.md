# ADR 0003 – Sound-Ansatz: Impulsmodell mit Wellenleiter-Auspuff

- Status: angenommen (Spike, Iteration 3)
- Datum: 2026-11-18

## Kontext

Realistischer Motorsound ist Priorität P1 (plan.md §1). Ein Reihensechser muss wie ein
Reihensechser klingen, und Tuning (Last, Zündung, Boost) soll den Klang verändern. Das
Mean-Value-Modell (Iteration 2) kennt keine einzelnen Zündungen. Der Sound braucht daher ein
eigenes, kurbelwinkelaufgelöstes Modell, das im Audio-Thread (48 kHz) läuft (research.md §6.1).

## Optionen

| Option | Vorteil | Nachteil |
|---|---|---|
| A. Samples/Granular (aufgenommene Motoren abspielen, Tonhöhe verschieben) | klingt sofort echt | kein Bezug zur Physik, Tuning verändert nichts, Lizenz der Aufnahmen |
| B. **Impulsmodell + digitaler Wellenleiter** (research.md §6.2-6.4) | physikalisch begründet (Zündfolge, Rohrlängen), billig, deterministisch, testbar | Klangqualität hängt an Abstimmung; Quelle ist synthetisch |
| C. engine-sim als Adapter (MIT) | sehr guter Klang | grosse Abhängigkeit, eigener Physik-Kern, schwer in unsere Architektur einzufügen |
| D. Zylinderaufgelöstes 0D-Modell als Quelle | Quelle direkt aus Thermodynamik | aufwendig; ist für Iteration 11 geplant |

## Entscheid

**Option B** als Stufe A (research.md §6.2):
- Kurbelwinkel pro Sample integrieren, EVO-Ereignisse subsample-genau auslösen.
- Bandbegrenzter Puls (Raised-Cosine mit schnellem Anstieg) pro Zylinder, Amplitude aus der Last,
  Zyklusschwankungen ±5 % und ±0,5° KW mit festem Seed.
- Auspuff als Netz digitaler Wellenleiter: Krümmerrohre, Streuknoten an Sammlern, Y-Rohr oder
  getrennte Bänke (Stereo), offenes Ende mit r = −Tiefpass.
- Schalldämpfer als kurze FIR-Faltung (256 Taps, synthetische IR), lastabhängiger Tiefpass,
  DC-Blocker, Soft-Clipper.
- Die Quelle ist hinter einer klaren Schnittstelle (`PulseSource` → Quellsignal pro Zylinder)
  austauschbar. In Iteration 11 ersetzt der Auslassmassenstrom des 0D-Modells den Puls (§7.5).
  Option C bleibt als späterer Adapter hinter demselben Port möglich (plan.md §9).

## Konsequenzen

- (+) Zündfolge und Abgasanlage sind hörbar und messbar: Die Ordnungsanalyse in den Tests zeigt
  2. Ordnung für I4, 3. Ordnung für I6, halbe Ordnungen pro Bank beim Crossplane-V8.
- (+) Release-Build rendert etwa 50× schneller als Echtzeit: viel Reserve für Lader-Geräusche.
- (+) Deterministisch, offline renderbar (WAV), ohne Audio-Hardware testbar (CI).
- (−) Die Pulsform ist eine Annahme. Klangfarbe und Lautstärkeverhältnisse sind abgestimmt,
  nicht gemessen.
- (−) Kein Ansaug- und Mechanikgeräusch (§6.5) im Spike. Kommt mit Iteration 7 oder später.
- (−) Direkte FIR-Faltung reicht nur für kurze IRs. Für gemessene, lange IRs braucht es
  partitionierte FFT-Faltung (§6.4).
