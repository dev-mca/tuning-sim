# ADR 0013 – 3D-Ansicht der ECU-Tabellen

- Status: angenommen
- Datum: 2026-11-28

## Kontext

Iteration 13 (plan.md) sieht ein 3D-Kennfeld mit three.js + react-three-fiber vor (§3). Der
2D-Editor (ADR 0005) bleibt das Werkzeug zum Bearbeiten; die 3D-Ansicht soll die Form einer
Tabelle (Täler, Kanten, Ausreisser) sichtbar machen.

## Entscheide

1. **Nur Anzeige** (Drehen, Zoomen, Verschieben, Betriebspunkt). Bearbeiten in 3D ist ein
   späterer Schritt (Backlog).
2. **three + @react-three/fiber, ohne drei.** `OrbitControls` kommt direkt aus three; weniger
   Abhängigkeiten.
3. **Eigener Chunk per `React.lazy`.** Die Hauptdatei wächst nur um ~4 kB; der 3D-Chunk
   (~945 kB, gzip ~250 kB) lädt, wenn das Panel erscheint. Ohne WebGL zeigt eine Error Boundary
   einen Hinweis.
4. **Gleiche Farben wie die Heatmap:** Fläche unbeleuchtet mit Vertex-Farben aus derselben
   Farbskala (`sequentialColor`, `valueRange`), Tone Mapping aus. Ein Gitter zeigt die Form.
5. **Betriebspunkt per `useFrame` + `store.latestState()`** (kein 60-Hz-React-State, ADR 0005).
6. **Reine Logik getrennt:** `surfaceModel.ts` (Skalen, Vertices, Farben, Ticks) mit Tests;
   die three-Komponenten bleiben dünn und ohne DOM-Tests.

## Folgen

- Vite warnt beim Build vor einem Chunk > 500 kB (gewollt, siehe 3).
- Die 3D-Ansicht hat eigene Tabellen-Tabs, weil `TablePanel` die Auswahl lokal hält. Gemeinsame
  Auswahl mit dem Editor steht im Backlog.
- Achsen nach echten Werten (ungleiche rpm-Abstände sichtbar), Höhe von min bis max der Tabelle.
