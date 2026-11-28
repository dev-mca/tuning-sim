# interface.md – WebSocket-Protokoll v1 (simcore ↔ UI)

Stand: Iteration 7b (neu: `setAudio`, `hello.audio`, Schub-Einstellungen, `catalyst_C`). Implementierung: `core/src/adapters/protocol.cpp` (Codec) und
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
| `setAudio` | `{ "volume": 0.0..1.0, "mute": false }` (volume wird geklemmt, `mute` optional) | `ack` (auch ohne Audiogerät) |
| `getOverrunSettings` | `{}` | `overrunSettings` |
| `setOverrunSettings` | `{ "fuelCut": false, "retard_deg": 35, "enrichment_pct": 10 }` (retard 0..60, enrichment 0..50) | `ack` oder `error`; danach `overrunSettings` an alle |
| `getLaunchSettings` | `{}` | `launchSettings` |
| `setLaunchSettings` | `{ "enabled": true, "launch_rpm": 4000, "retard_deg": 10 }` (launch 2000..6500, retard 0..30) | `ack` oder `error`; danach `launchSettings` an alle |
| `setLaunchSwitch` | `{ "held": true }` (Launch-Taste, wie das Pedal ein Live-Eingang, nicht Teil des Tunes) | `ack` |
| `shift` | `{ "gear": 2 }` (0 = Leerlauf, 1..6; sequenziell mit automatischer Kupplung) | `ack` oder `error` (ungültiger Gang, Schaltung läuft, Dyno-Lauf) |
| `setClutch` | `{ "pressed": true }` (Kupplungspedal; getreten = Kupplung offen) | `ack` |
| `setBrake` | `{ "value": 0.0..1.0 }` (Bremspedal, wird geklemmt; Live-Eingang wie das Gaspedal) | `ack` |
| `startRollerRun` | `{ "gear": 4, "rpmFrom": 1500, "rpmTo": 6800 }` (`rpmFrom`/`rpmTo` optional) | `ack` oder `error`; danach `dynoProgress` und am Ende `dynoResult` mit `kind: "roller"` an alle |

- `x`/`y` sind Indizes (Spalte = Drehzahl-Bin, Zeile = MAP-Bin), `value` in der Einheit der Tabelle.
- `setTableCells` gilt ganz oder gar nicht: Ist eine Zelle ungültig, wird nichts geändert.
- `ignitionMbt` ist eine Motoreigenschaft und lässt sich nur lesen.
- Während eines Sweeps steuert der Dyno die Drehzahl, und das Pedal folgt `startSweep.pedal`.
  Nach dem Sweep läuft der Motor wieder frei mit dem zuletzt gesetzten Pedalwert.
- Seit Iteration 10 steht das Auto immer auf dem Rollenprüfstand (research.md §8.4). Im Leerlauf
  oder mit getretener Kupplung dreht der Motor frei wie bisher. `startSweep` (Motorprüfstand) nimmt
  den Gang heraus. Während eines Sweeps oder Rollenlaufs sind `shift` und Launch wirkungslos bzw.
  abgelehnt; `stopSweep` beendet beide Läufe.
- Der 2-Step ist scharf, wenn die Launch-Taste gehalten **oder** die Kupplung im Stand (< 5 km/h)
  getreten ist.

## simcore → UI

### `hello` (einmal pro Verbindung)
```json
{ "v": 1, "type": "hello", "payload": {
  "protocolVersion": 1, "simcore": "0.1.0", "stateRate_Hz": 60,
  "engine": { "id": "i6_30_na", "name": "...", "cylinders": 6, "displacement_l": 2.993,
              "idle_rpm": 750, "revLimit_rpm": 7000 },
  "tables": [ { "id": "ve", "name": "Volumetric Efficiency", "unit": "%", "editable": true }, ... ],
  "audio": { "available": true, "device": "Speakers (PulseAudio)" } } }
```
`audio.available` ist `false`, wenn simcore mit `--no-audio` läuft oder kein Gerät öffnen konnte;
`device` nennt dann den Grund. Den Ton spielt simcore selbst ab, nie über diese Schnittstelle.

### `state` (60 Hz, an alle)
```json
{ "v": 1, "type": "state", "payload": {
  "t_s": 12.345, "rpm": 3012.4, "pedal": 0.4, "map_kPa": 78.1, "lambda": 1.0, "lambdaTarget": 1.0,
  "ignition_deg": 31.5, "torque_Nm": 0.8, "power_kW": 0.25, "airflow_kg_s": 0.051,
  "fuelflow_kg_s": 0.0035, "egt_C": 742.0, "dynoMode": "free", "sweepProgress": 0.0,
  "cutActive": false } }
```
`dynoMode`: `free` (kein Prüfstandslauf: Motor frei oder Fahren auf der Rolle), `sweep` (Motorprüfstand)
oder `roller` (Rollenlauf). `torque_Nm` ist das effektive Moment
des Motors, `cutActive` meldet eine aktive Kraftstoffabschaltung (Drehzahlbegrenzer oder Schubabschaltung).
Seit Iteration 7b zusätzlich `catalyst_C` (Kat-Temperatur) und `overrunFueling` (die ECU spritzt im
Schub ein, weil die Schubabschaltung aus ist: Pops möglich).
Seit Iteration 7c zusätzlich `launchSwitch` (Launch-Taste gedrückt; während eines Sweeps wirkungslos),
`launchActive` (2-Step arbeitet: eingeschaltet, Taste gedrückt, Gas offen) und `sparkCut` (der 2-Step
unterbricht gerade die Zündung, spritzt aber weiter ein).
Seit 10a stehen die vereinbarten Felder für spätere Iterationen schon im `state` (plan.md §11); `null`
heisst «nicht vorhanden»: `iat_C`, `boost_kPa`, `turboSpeed_rpm`, `wastegateDuty_pct` (Iteration 8),
`knockRetard_deg` (0 ohne Klopfen), `gear` (0 = Leerlauf, 1.. = Gänge), `clutch` (0 = offen .. 1 =
geschlossen), `vehicleSpeed_kmh`, `wheelTorque_Nm` (Iteration 10).
Seit Iteration 10 sind `gear`, `clutch`, `vehicleSpeed_kmh` und `wheelTorque_Nm` immer Zahlen, dazu
`clutchLocked` (Motor und Auto drehen gemeinsam) und `shifting` (Zugkraftunterbrechung beim Schalten).
Seit 10b zusätzlich `brake` (Bremspedal 0..1).

### `overrunSettings` (Antwort auf `getOverrunSettings`, Broadcast nach `setOverrunSettings`)
```json
{ "v": 1, "type": "overrunSettings", "payload": { "fuelCut": true, "retard_deg": 0, "enrichment_pct": 0 } }
```
`fuelCut` = Schubabschaltung (Serie: an). Ist sie aus, spritzt die ECU im Schub weiter ein, zündet um
`retard_deg` später als das Kennfeld und gibt `enrichment_pct` mehr Kraftstoff (research.md §3.3, §2.10a).

### `launchSettings` (Antwort auf `getLaunchSettings`, Broadcast nach `setLaunchSettings`)
```json
{ "v": 1, "type": "launchSettings", "payload": { "enabled": false, "launch_rpm": 4000, "retard_deg": 0, "flatShift": false } }
```
`flatShift` (seit Iteration 10, beim Setzen optional): beim Hochschalten mit offenem Gas unterbricht die ECU
die Zündung, statt dass der Motor hochdreht.
Launch Control / 2-Step (research.md §3.4, Serie: aus). Mit gedrückter Launch-Taste und offenem Gas zündet
die ECU um `retard_deg` später und hält `launch_rpm` mit Zündunterbrechung; der weiter eingespritzte
Kraftstoff kann im heissen Auspuff knallen.

### `table` (Antwort auf `getTable`)
Gleiches Format wie die Dateien in `shared/tables/` (plan.md §5.1), dazu `editable`:
```json
{ "v": 1, "type": "table", "id": "7", "payload": {
  "id": "ve", "name": "Volumetric Efficiency", "unit": "%", "editable": true,
  "xAxis": { "name": "rpm", "unit": "1/min", "bins": [800, ...] },
  "yAxis": { "name": "map", "unit": "kPa", "bins": [20, ...] },
  "min": 0, "max": 150, "values": [[...16 Werte...], ...16 Zeilen...] } }
```
`values[y][x]`, Zeile 0 = tiefster MAP. Achsenwerte (`1/min`, `kPa`) sind auf 6 Dezimalen gerundet.

### `ack` / `error`
```json
{ "v": 1, "type": "ack", "id": "7", "payload": {} }
{ "v": 1, "type": "error", "id": "7", "payload": { "message": "unknown table 'turbo'" } }
```

### `dynoProgress` (10 Hz während eines Sweeps, an alle)
```json
{ "v": 1, "type": "dynoProgress", "payload": { "progress": 0.42, "rpm": 3520.0 } }
```

### `dynoResult` (am Ende eines Sweeps oder Rollenlaufs, an alle)
Parallele Arrays, ein Eintrag pro 100 1/min. `kind`: `engine` (Motorprüfstand, `gear` = `null`) oder
`roller` (Rollenlauf im Gang `gear`: `power_kW` = Radleistung, `torque_Nm` = Radmoment / Gesamtübersetzung,
also um die Verluste im Antriebsstrang kleiner als das Motormoment).
```json
{ "v": 1, "type": "dynoResult", "payload": { "kind": "engine", "gear": null,
  "rpm": [...], "torque_Nm": [...], "power_kW": [...], "lambda": [...], "map_kPa": [...],
  "ignition_deg": [...], "egt_C": [...], "airflow_kg_s": [...] } }
```

## Vertrag (Beispiele)

`shared/protocol/v1/server/` und `client/` enthalten je ein vollständiges Beispiel pro Nachrichtentyp.
`core/tests/protocol_contract_test.cpp` und `ui/src/protocol/contract.test.ts` prüfen beide Seiten gegen
dieselben Dateien (Feldnamen, Verschachtelung, JSON-Typen). Neue oder geänderte Nachricht: zuerst das
Beispiel, dann C++ und TypeScript, dann dieses Dokument.

## Versionierung

Neue Felder und Nachrichtentypen dürfen in v1 hinzukommen; Clients ignorieren Unbekanntes.
Inkompatible Änderungen (Felder umbenennen oder entfernen, Einheiten ändern) erhöhen `v`.

## Geplant (spätere Iterationen)

`setDynoMode` (holdRpm), `loadEngine`.
Tunes speichert und lädt die UI selbst (ADR 0006); beim Laden sendet sie die abweichenden Zellen
per `setTableCells`, ein eigenes `saveTune`/`loadTune` ist daher nicht vorgesehen.

## Verhalten des UI-Clients (Iteration 6)

- Reconnect mit Backoff 0,5 → 1 → 2 → 4 → 5 s; Befehle ohne Verbindung werden verworfen.
- Nach jedem `hello` holt die UI alle Tabellen mit `getTable`, fragt `getOverrunSettings` und sendet Pedal und `setAudio` erneut.
- Auf ein `error` zu `setOverrunSettings` fragt die UI die Einstellungen neu ab.
- Auf ein `error` zu `setTableCells` holt die UI die betroffene Tabelle neu.
- Ungültige Frames (kein JSON, falsche Version, fehlende Pflichtfelder) werden verworfen und
  in der Browser-Konsole gemeldet.
