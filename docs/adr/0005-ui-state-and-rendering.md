# ADR 0005 – UI: Zustand, Rendering und Dyno-Darstellung

- Status: angenommen
- Datum: 2026-11-19

## Kontext

Iteration 5 baut das Cockpit mit Fake-Daten (plan.md §8). Der Motorzustand kommt mit 60 Hz,
die Nadeln sollen mit der Bildschirmrate (60 bis 144 Hz) flüssig laufen, und der Tabelleneditor
zeichnet 256 Zellen. In Iteration 6 kommt der WebSocket-Client dazu, ohne dass sich die
Komponenten ändern sollen.

## Optionen

**Zustandsverwaltung**

| Option | Vorteil | Nachteil |
|---|---|---|
| **Eigener Store + `useSyncExternalStore`** | keine Abhängigkeit, lehrreich, volle Kontrolle darüber, was rendert | etwas Handarbeit (Snapshot, Listener) |
| Zustand/Redux | bekannte Muster, DevTools | zusätzliche Bibliothek für wenige Zustände |
| 60-Hz-Zustand in React-State | am einfachsten | React rendert 60-mal pro Sekunde alles neu, das kostet unnötig Leistung |

**Dyno-Graph**

| Option | Vorteil | Nachteil |
|---|---|---|
| **Zwei Graphen (Moment, Leistung) mit gekoppeltem Cursor** | eine Y-Achse pro Einheit, keine Schein-Schnittpunkte | braucht mehr Höhe |
| Ein Graph mit zwei Y-Achsen (klassischer Prüfstandsausdruck) | kompakt, vertraut | Schnittpunkte der Kurven hängen nur von der Achsenskalierung ab, die Kurven verleiten deshalb zu Fehlschlüssen |

## Entscheid

- **Port `SimSource`** (`ui/src/sim/SimSource.ts`). `FakeSimSource` implementiert ihn jetzt, der
  WebSocket-Client in Iteration 6. Die Fake-Quelle spricht Protokoll v1 (docs/interface.md)
  asynchron und liefert tiefe Kopien, sie verhält sich also wie eine echte Verbindung.
- **Eigener `SimStore`**. React sieht nur einen selten wechselnden Snapshot (Tabellen, Läufe,
  Sweep-Status, Flags). Der 60-Hz-Zustand liegt ausserhalb von React (`latestState()`).
  Gauges, Live-Cursor und Digitalwerte lesen ihn per `requestAnimationFrame` und schreiben
  direkt über Refs ins SVG bzw. Canvas. Glättung erster Ordnung mit `1 − exp(−dt/τ)`, damit die
  Bildrate keine Rolle spielt.
- **Tabellen als Canvas** mit zwei Ebenen: Das Raster wird nur bei Änderungen neu gezeichnet,
  der Live-Cursor auf einer eigenen Ebene pro Frame. Die Bearbeitungslogik besteht aus reinen
  Funktionen (`ui/src/domain/tableEdit.ts`), die Zellenlisten liefern. Dieselbe Liste geht an
  `setTableCells` und in die Undo-Historie.
- **Dyno: zwei uPlot-Graphen** mit gemeinsamem Cursor (`cursor.sync`). Jeder Lauf behält seine
  Farbe (feste Reihenfolge, 8 Farben, gegen Farbfehlsichtigkeit geprüft). Ab dem 9. Lauf fällt
  der älteste weg.
- **Tests mit Vitest 5.1** für Logik, Fake-Quelle und Store. Komponententests mit DOM gibt es vorerst
  nicht. Die Komponenten bleiben dünn, und das Aussehen wird per Screenshot geprüft.

## Konsequenzen

- (+) Iteration 6 ersetzt in `App.tsx` nur die Quelle. Store und Komponenten bleiben gleich.
- (+) Flüssige Nadeln ohne React-Renders pro Frame.
- (+) Keine zusätzliche Bibliothek für den Zustand.
- (−) Imperativer Code in den Animations-Callbacks (Refs, `setAttribute`), weniger «React-typisch».
- (−) Canvas-Farben stehen doppelt: in `index.css` (`@theme`) und `ui/src/ui/theme/palette.ts`.
- (−) Der Fake-Dyno skaliert nur die Golden-Kurve mit den Wirkungsgradfaktoren aus research.md
  §2.6 und §2.7. Er ist kein zweiter Simulator, die echte Wirkung zeigt erst simcore.
