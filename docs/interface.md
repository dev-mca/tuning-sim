# interface.md – WebSocket-Protokoll v1 (simcore ↔ UI)

Stand: Iteration 4. Implementierung: `core/src/adapters/protocol.cpp` (Codec) und
`core/src/adapters/websocket_server.cpp` (Transport). Testclient: `core/tools/ws_test.html`.

## Verbindung

- `ws://127.0.0.1:47800` (Port mit `simcore --port <n>` änderbar). simcore lauscht **nur** auf
  127.0.0.1, ist also von anderen Rechnern aus nicht erreichbar.
- Text-Frames mit JSON (UTF-8). Audio geht nie über diese Schnittstelle (ADR 0001).
- Beim Verbinden sendet simcore sofort `hello`, danach `state` mit etwa 60 Hz an alle Clients.

## Hülle (Envelope)

```json
{ "v": 1, "type": "<typ>", "id": "<optional>", "payload": { } }
```

| Feld | Bedeutung |
|---|---|
| `v` | Protokollversion, immer `1`. Andere Werte werden mit `error` beantwortet. |
| `type` | Nachrichtentyp (siehe unten) |
| `id` | Optionale Request-ID des Clients. Antworten (`ack`, `error`, `table`) tragen dieselbe `id`. |
| `payload` | Inhalt, je nach Typ. Fehlt er, gilt `{}`. |

**Einheiten:** Drehzahl in 1/min, Druck in kPa, Leistung in kW, Temperaturen in °C, VE in %,
Zündung in °v. OT. Intern rechnet simcore in SI; umgerechnet wird nur im Codec.
Nicht definierte Werte (z. B. λ während der Schubabschaltung) werden als `null` gesendet.

## UI → simcore

| type | payload | Antwort |
|---|---|---|
| `setPedal` | `{ "value": 0.0..1.0 }` (wird auf 0..1 geklemmt) | `ack` |
| `startSweep` | `{ "rpmFrom": 1000, "rpmTo": 7000, "rate_rpm_s": 300, "pedal": 1.0 }` (`rate_rpm_s` und `pedal` optional) | `ack` oder `error`; danach `dynoProgress` (10 Hz) und am Ende `dynoResult` an alle |
| `stopSweep` | `{}` | `ack` |
| `getTable` | `{ "id": "ve" }` (`ve`, `lambdaTarget`, `ignition`, `ignitionMbt`) | `table` oder `error` |
| `setTableCells` | `{ "id": "ignition", "cells": [ { "x": 3, "y": 10, "value": 28.5 } ] }` | `ack` oder `error` |

- `x`/`y` sind Indizes (Spalte = Drehzahl-Bin, Zeile = MAP-Bin), `value` in der Einheit der Tabelle.
- `setTableCells` gilt ganz oder gar nicht: Ist eine Zelle ungültig, wird nichts geändert.
- `ignitionMbt` ist eine Motoreigenschaft und lässt sich nur lesen.
- Während eines Sweeps steuert der Dyno die Drehzahl, und das Pedal folgt `startSweep.pedal`.
  Nach dem Sweep läuft der Motor wieder frei mit dem zuletzt gesetzten Pedalwert.

## simcore → UI

### `hello` (einmal pro Verbindung)
```json
{ "v": 1, "type": "hello", "payload": {
  "protocolVersion": 1, "simcore": "0.1.0", "stateRate_Hz": 60,
  "engine": { "id": "i6_30_na", "name": "...", "cylinders": 6, "displacement_l": 2.993,
              "idle_rpm": 750, "revLimit_rpm": 7000 },
  "tables": [ { "id": "ve", "name": "Volumetric Efficiency", "unit": "%", "editable": true }, ... ] } }
```

### `state` (60 Hz, an alle)
```json
{ "v": 1, "type": "state", "payload": {
  "t_s": 12.345, "rpm": 3012.4, "pedal": 0.4, "map_kPa": 78.1, "lambda": 1.0, "lambdaTarget": 1.0,
  "ignition_deg": 31.5, "torque_Nm": 0.8, "power_kW": 0.25, "airflow_kg_s": 0.051,
  "fuelflow_kg_s": 0.0035, "egt_C": 742.0, "dynoMode": "free", "sweepProgress": 0.0,
  "cutActive": false } }
```
`dynoMode`: `free` (Motor läuft ohne Bremse) oder `sweep`. `torque_Nm` ist das effektive Moment
des Motors, `cutActive` meldet eine aktive Kraftstoffabschaltung (Drehzahlbegrenzer).

### `table` (Antwort auf `getTable`)
Gleiches Format wie die Dateien in `shared/tables/` (plan.md §5.1), dazu `editable`:
```json
{ "v": 1, "type": "table", "id": "7", "payload": {
  "id": "ve", "name": "Volumetric Efficiency", "unit": "%", "editable": true,
  "xAxis": { "name": "rpm", "unit": "1/min", "bins": [800, ...] },
  "yAxis": { "name": "map", "unit": "kPa", "bins": [20, ...] },
  "min": 0, "max": 150, "values": [[...16 Werte...], ...16 Zeilen...] } }
```
`values[y][x]`, Zeile 0 = tiefster MAP.

### `ack` / `error`
```json
{ "v": 1, "type": "ack", "id": "7", "payload": {} }
{ "v": 1, "type": "error", "id": "7", "payload": { "message": "unknown table 'turbo'" } }
```

### `dynoProgress` (10 Hz während eines Sweeps, an alle)
```json
{ "v": 1, "type": "dynoProgress", "payload": { "progress": 0.42, "rpm": 3520.0 } }
```

### `dynoResult` (am Ende eines Sweeps, an alle)
Parallele Arrays, ein Eintrag pro 100 1/min:
```json
{ "v": 1, "type": "dynoResult", "payload": {
  "rpm": [...], "torque_Nm": [...], "power_kW": [...], "lambda": [...], "map_kPa": [...],
  "ignition_deg": [...], "egt_C": [...], "airflow_kg_s": [...] } }
```

## Versionierung

Neue Felder und Nachrichtentypen dürfen in v1 hinzukommen; Clients ignorieren Unbekanntes.
Inkompatible Änderungen (Felder umbenennen oder entfernen, Einheiten ändern) erhöhen `v`.

## Geplant (spätere Iterationen)

`setDynoMode` (holdRpm), `loadEngine`, `saveTune`/`loadTune` (Iteration 6), `setAudio` (7),
`shift`/`setClutch` (10).
