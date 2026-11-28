# ADR 0012 – Import von TunerStudio-.msq-Dateien

- Status: angenommen (2026-11-28, vorher vorgeschlagen)
- Nachtrag beim Merge: Die vorgeschlagenen Standardwerte bleiben (eigene Achsen + Umrechnung, stoich 14,7 mit Warnung, unbekannte Einheiten werden abgelehnt). Zeilenreihenfolge mit einer echten TunerStudio-Datei prüfen.
- Datum: 2026-11-28

## Kontext

Iteration 13 (plan.md) verlangt einen .msq-Import. Eine .msq ist XML: `<msq>` → `<page>` →
`<constant name rows cols units>` mit Werten in Anzeigeeinheiten, getrennt durch Leerraum. Die
Achsen sind eigene Konstanten (z. B. `veTable` + `rpmBins` + `fuelLoadBins`). Unsere Tabellen
haben feste Achsen (16 × 16 aus `shared/tables`), und simcore kennt keine Achsenänderung.

## Entscheide

1. **Reine Funktionen in `ui/src/domain/msq/`**: `parseMsq` (XML → typisierte Konstanten),
   `msqMapping` (welche Konstante → welche Tabelle, Einheiten), `importMsq` (→ `TuneFile`).
   Das Ergebnis läuft durch den bestehenden Lade-Pfad (`store.importTune`, ADR 0006): gleiche
   Prüfungen, **ein** Undo-Schritt, Zellen per `setTableCells`. Keine Protokolländerung.
2. **XML mit dem `DOMParser` der Plattform.** Im Browser vorhanden; Vitest läuft in Node, daher
   bekommen nur die msq-Tests `// @vitest-environment jsdom` (jsdom 31.0.1, MIT, nur
   devDependency). Kein eigener XML-Parser (Entities, Kommentare, Namespaces wären Handarbeit).
3. **Zuordnung** (nur Namen, kein Firmware-Code, keine .ini-Inhalte übernommen):

   | Unsere Tabelle | Speeduino-Stil | rusEFI-Stil |
   |---|---|---|
   | `ve` (%) | `veTable`, `rpmBins`, `fuelLoadBins` | `veTable`, `veRpmBins`, `veLoadBins` |
   | `ignition` (deg BTDC) | `advTable1`, `rpmBins2`, `mapBins1` | `ignitionTable`, `ignitionRpmBins`, `ignitionLoadBins` |
   | `lambdaTarget` (λ) | `afrTable`, `rpmBinsAFR`, `loadBinsAFR` | `lambdaTable`, `lambdaRpmBins`, `lambdaLoadBins` |

   Gewählt wird die erste Variante, deren drei Konstanten alle vorhanden sind. Fehlt eine
   Tabelle in der Datei, bleibt sie unverändert (Warnung). Fehlt alles, wird abgelehnt.
4. **Einheiten nur am Rand** (`msqMapping.ts`): x = rpm, y = MAP in kPa, VE in %, Zündung in
   Grad, Lambda als `lambda` oder `AFR`. AFR → λ mit **14,7 (Benzin)**; weicht die Konstante
   `stoich` der Datei davon ab, gibt es eine Warnung. Unbekannte oder fehlende Einheiten,
   Alpha-N-Last (TPS %), falsche Grössen und nicht steigende Achsen werden mit klarer Meldung
   abgelehnt.
5. **Zeilenreihenfolge:** Zeile 0 der Datei = kleinster Last-Bin (Speicherreihenfolge des
   Steuergeräts), wie `values[0]` bei uns.
6. **Unsere Achsen bleiben.** Andere Achsen werden **bilinear mit Klemmen** auf unser Raster
   umgerechnet (`interpolate` aus `table2d.ts`, dieselbe Regel wie simcore, research.md §2.3)
   und als Warnung gemeldet, inkl. Hinweis, wenn unser Raster über die Datei hinausreicht
   (Randwerte gehalten). Werte werden auf 3 Nachkommastellen gerundet. Werte ausserhalb
   min…max lehnt der bestehende Lade-Pfad ab.

## Verworfene Optionen

- Achsen aus der Datei übernehmen: bräuchte ein neues Protokoll-Kommando und Änderungen an
  simcore, und die Kennfelder würden zum Golden File nicht mehr passen.
- Eigener Mini-XML-Parser oder `fast-xml-parser` in Produktion: zusätzlicher Code bzw.
  Laufzeit-Abhängigkeit, obwohl der Browser einen Parser mitbringt.

## Konsequenzen

- (+) Keine Änderung an simcore, Protokoll, Store oder `tuneFile.ts`.
- (−) Umrechnen glättet: Feine Details zwischen unseren Bins gehen verloren, Bereiche ausserhalb
  der Datei sind nur gehaltene Randwerte.
- (−) Nur die drei Tabellen, nur MAP-Last, nur Benzin. Weitere Konstanten (Anreicherungen,
  Begrenzer) werden ignoriert.
