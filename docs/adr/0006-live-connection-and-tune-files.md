# ADR 0006 – Live-Verbindung und Tune-Dateien

- Status: angenommen
- Datum: 2026-11-19

## Kontext

Iteration 6 verbindet die UI mit simcore (plan.md §8). Gefordert sind ein WebSocket-Client mit
Reconnect, das Umschalten zwischen Fake und Live sowie Tune speichern/laden (JSON). plan.md §6
nennt dafür die Nachrichten `saveTune`/`loadTune`. simcore hat aber keinen Ort für
Benutzerdateien, und die UI wird später in Tauri laufen.

## Optionen (Tune-Dateien)

| Option | Vorteil | Nachteil |
|---|---|---|
| **UI speichert/lädt (Browser-Download bzw. Dateiauswahl), simcore bekommt die Zellen per `setTableCells`** | keine Protokolländerung, kein Dateisystemzugriff in simcore, in Tauri durch native Dialoge ersetzbar | grosse Tunes gehen als Zellenlisten über die Leitung (für 3 × 256 Zellen unkritisch) |
| `saveTune`/`loadTune` in simcore | simcore kennt den Tune als Ganzes | Pfade, Rechte und Dateiauswahl im Headless-Prozess, neue Fehlerfälle im Protokoll |

## Entscheid

- **Tune-Dateien in der UI** (`ui/src/domain/tuneFile.ts`). Format `tuning-sim-tune` v1 mit
  Motor-ID, Zeitpunkt und den editierbaren Tabellen samt Achsen. Beim Laden wird zuerst alles
  geprüft (Motor, Tabellen vorhanden und editierbar, gleiche Achsen mit Toleranz 1e-9, Werte in
  min…max). Danach gehen nur die abweichenden Zellen als **ein** Undo-Schritt an simcore.
  Achsen werden nicht umgerechnet: Ein Tune für ein anderes Raster wird abgelehnt.
  `saveTune`/`loadTune` entfallen aus dem Protokoll-Plan.
- **`WebSocketSimSource`** implementiert den Port aus ADR 0005. Jeder Frame wird geprüft
  (`protocol/parse.ts`), unbekannte Typen ignoriert. Reconnect mit Backoff 0,5 → 1 → 2 → 4 → 5 s.
  Befehle ohne Verbindung werden verworfen, nicht gepuffert.
- **simcore ist die Quelle der Wahrheit:** Nach jedem `hello` lädt der Store alle Tabellen neu
  und sendet das Pedal erneut. Lehnt simcore ein `setTableCells` ab, holt der Store die Tabelle
  neu (die optimistische lokale Änderung wäre sonst falsch).
- **Umschalten Live/Fake** baut einen neuen Store. Läufe und Undo-Historie der anderen Quelle
  werden bewusst nicht übernommen. Startquelle: `?source=` in der URL, sonst die letzte Wahl
  (localStorage), sonst Live.
- **End-to-End-Test** (`ui/src/e2e/live.e2e.ts`, `npm run test:e2e`) gegen ein echtes simcore,
  auch in CI: Zündung −5° → Spitzenmoment sinkt.

## Konsequenzen

- (+) Keine Änderung an simcore nötig. Das Protokoll v1 bleibt unverändert.
- (+) Abnahme automatisch prüfbar, inkl. CI.
- (−) «Original» in der UI ist der Stand von simcore beim ersten Verbinden, nicht zwingend der
  ausgelieferte Tune. Hat ein früherer Client simcore verändert, gilt das als Original. Backlog:
  simcore liefert den Basis-Tune mit.
- (−) simcore nimmt WebSocket-Verbindungen von jeder Webseite an, die im selben Browser offen ist
  (kein Origin-Check). Für ein lokales Lernprojekt tragbar. Backlog: Origin prüfen.
