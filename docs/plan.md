# plan.md – tuning-sim

Dieser Plan beschreibt das gesamte Projekt: Ziel, Architektur, Tools, Schnittstellen, Implementierungsansatz und die Iterationen mit Prioritäten und Abnahmekriterien. Claude Code liest ihn zu Beginn jeder Session und arbeitet danach. Die Theorie und Mathematik stehen in `docs/research.md`. Hier steht, **was** gebaut wird und **wie**, dort steht, **warum es physikalisch so funktioniert**.

---

## 1. Ziel und Prioritäten

Ein Desktop-Simulator, mit dem ich Motor-Tuning lerne, ohne an einem echten Auto zu schrauben.

| Prio | Ziel | Bedeutung |
|---|---|---|
| **P1** | Nachvollziehbare Physik | ECU-Kennfelder, Luft/Kraftstoff, Zündung, Turbo, Dyno verhalten sich plausibel und erklärbar |
| **P1** | Realistischer Motorsound | Zylinderzahl, Zündfolge und Auspuff hörbar; Tuning (Last, Zündung, Boost) verändert den Klang |
| **P2** | Coole UI | Gauges (Drehzahl, Boost, Lambda, Temperaturen), editierbare ECU-Tabellen (Heatmap + 3D), Dyno-Graphen vorher/nachher |
| **P2** | Aufladung | Turbo (inkl. Pfeifen, BOV, Verdichterpumpen) und Kompressor/Supercharger (inkl. Heulen) |
| **P3** | Antriebsstrang | Getriebe, Kupplung, Fahrzeug, Rollenprüfstand; Launch Control, Flat Shift, Pops & Bangs |
| **P3** | Desktop-App | Installierbares Paket mit Tauri v2 |
| **P4** | Extras | Einfaches 3D-Auto, .msq-Import (TunerStudio), mehrere Motor-Presets |

**Nicht-Ziele:** kein Fahrspiel mit Strecke, keine physikalisch exakte Ingenieurssoftware, kein Ersatz für echtes Tuning. Ein Tune, der hier funktioniert, ist keine Freigabe für ein echtes Auto.

**Leitprinzip:** Jede Iteration liefert etwas Sicht- oder Hörbares. Pro Iteration kommt höchstens **eine** neue Hauptdisziplin dazu (z. B. nicht gleichzeitig Audio-DSP und React lernen).

---

## 2. Architektur

### 2.1 Stil

- **Monorepo**, **hexagonale Architektur** (Ports & Adapters). Eine Anwendung, zwei Prozesse (simcore + UI), kein Microservice.
- Die Abhängigkeit zeigt nach innen: `domain` kennt nichts von Audio, Netzwerk oder Dateien. Adapter implementieren Ports.
- simcore ist ein eigenständiger Headless-Prozess. Die WebSocket-Schnittstelle ist **dauerhaft**, kein Provisorium. Tauri startet simcore später als Sidecar.

```
┌──────────────────────── simcore (C++-Prozess) ────────────────────────┐
│ domain/    Motor-MVM, ECU (Tabellen, Regler), Aufladung, Antrieb,      │
│            Dyno, Klopfmodell — reine Logik, keine I/O                  │
│ acoustic/  Kurbelwinkel-Ereignisse, Abgasimpulse, Wellenleiter-Auspuff,│
│            Faltung, Lader-Geräusche — läuft im Audio-Takt              │
│ ports/     IAudioSink, IStatePublisher, ICommandSource,                │
│            IEngineRepository, ITableRepository, IClock                 │
│ adapters/  MiniaudioSink, WebSocketServer, JsonFileRepository          │
│ app/       main(): Threads, Simulationsschleife, Verdrahtung           │
└────────────────────────────────────────────────────────────────────────┘
        ▲ WebSocket ws://127.0.0.1:47800 (JSON, Protokoll v1)
        ▼
┌──────────────── ui (React/TS im Browser, später in Tauri) ─────────────┐
│ Gauges · Tabelleneditor (Heatmap/3D) · Dyno-Graph · Steuerung · 3D-Auto │
└─────────────────────────────────────────────────────────────────────────┘
```

### 2.2 Threads in simcore

| Thread | Takt | Aufgabe |
|---|---|---|
| Simulation | 1 kHz fix (Δt = 1 ms) | Mean-Value-Modell, ECU, Regler, Aufladung, Antrieb, Dyno |
| Audio | 48 kHz (Callback von miniaudio, Puffer 256–512 Frames) | Kurbelwinkel integrieren, Impulse erzeugen, Auspuff, Mix |
| Netzwerk | ereignisgesteuert, Zustand mit 60 Hz senden | Befehle empfangen, Zustand/Ergebnisse senden |

**Kommunikation zwischen Threads:**
- Simulation → Audio: Struktur `AcousticParams` (Drehzahl, Last pro Zylinder, Zündwinkel, Lambda, Ladedruck, Laderdrehzahl, Flags wie Zündunterbrechung) über atomare Werte bzw. einen Double-Buffer mit atomarem Index. Der Audio-Thread interpoliert zwischen Updates, damit nichts knackt.
- Netzwerk → Simulation: Befehle in eine lock-freie Queue (SPSC). Die Simulation liest sie am Anfang jedes Schritts.
- Simulation → Netzwerk: Zustands-Snapshot per Double-Buffer.
- Der Audio-Callback alloziert nie, sperrt nie, macht kein I/O.

### 2.3 Simulationsschleife

- Fester Zeitschritt mit Akkumulator («Fix Your Timestep»). Echtzeit-Modus und «so schnell wie möglich»-Modus (für Dyno-Sweeps in Tests und für Golden Files).
- Determinismus: gleiche Eingaben → gleiche Ausgaben. Zufallszahlen (Zyklusschwankungen) nur mit festem Seed.
- Integrator: zuerst explizites Euler. Wechsel auf semi-implizites Euler oder RK4, wo Steifigkeit Probleme macht (Kupplung, Saugrohr) – siehe `research.md` §9.

---

## 3. Tools und Stack

| Bereich | Wahl | Hinweis |
|---|---|---|
| C++ | C++20 | MSVC / Clang / GCC aktuell |
| Build | CMake ≥ 3.25 mit `CMakePresets.json`, Ninja | Root-`CMakeLists.txt` bindet `core/` ein, damit CLion das Repo-Root öffnen kann |
| C++-Abhängigkeiten | CMake `FetchContent` | vcpkg nur nach Rückfrage |
| C++-Tests | doctest **oder** Catch2 v3 | Entscheid in Iteration 1 (ADR) |
| Audio-I/O | miniaudio | Single-Header, Public Domain/MIT-0 |
| JSON (C++) | nlohmann/json | MIT |
| WebSocket (C++) | Entscheid in Iteration 4 (ADR) | Kandidaten: IXWebSocket, uWebSockets, Boost.Beast |
| Lock-freie Queue | eigene SPSC-Ringpuffer-Klasse (Lernziel) | alternativ readerwriterqueue (BSD) |
| Python | 3.12+, numpy, matplotlib, pytest, ruff | in `lab/.venv` |
| UI | TypeScript (strict), React, Vite, Tailwind | ESLint + Prettier |
| Graphen | uPlot | Dyno-Kurven, Live-Zeitreihen |
| Gauges | eigenes SVG | Lernziel, volle Kontrolle über den Look |
| ECU-Tabellen | eigene Canvas-Heatmap-Komponente | zentrales Element, selbst bauen |
| 3D | three.js + react-three-fiber | 3D-Kennfeld, später Auto |
| Desktop | Tauri v2, simcore als Sidecar | Rust nur für Tauri-Hülle, kein FFI |
| IDE | CLion (C++), IntelliJ Ultimate (TS, Python) | |
| Formatierung | clang-format, Prettier, ruff | |

---

## 4. Repo-Struktur

```
tuning-sim/
├── CLAUDE.md
├── README.md
├── THIRD_PARTY_NOTICES.md
├── CMakeLists.txt            # Root: add_subdirectory(core)
├── CMakePresets.json
├── .gitignore  .editorconfig  .clang-format
├── docs/
│   ├── plan.md               # dieses Dokument
│   ├── research.md           # Physik, Mathematik, Quellen
│   ├── setup.md              # Tools + Versionen des Systems
│   ├── interface.md          # WebSocket-Protokoll (ab Iteration 4)
│   └── adr/                  # Architecture Decision Records
├── core/
│   ├── CMakeLists.txt
│   ├── include/simcore/      # öffentliche Header
│   ├── src/
│   │   ├── domain/           # engine/, ecu/, boost/, drivetrain/, dyno/, common/
│   │   ├── acoustic/
│   │   ├── ports/
│   │   ├── adapters/
│   │   └── app/
│   ├── tests/
│   └── spikes/               # Wegwerf-Experimente (z. B. sound_spike)
├── lab/
│   ├── pyproject.toml
│   ├── src/tuning_lab/
│   ├── notebooks/
│   └── tests/
├── ui/                       # Vite-Projekt
└── shared/
    ├── engines/              # Motordefinitionen (JSON)
    ├── tables/               # ECU-Tabellen (JSON)
    ├── impulses/             # Impulsantworten (WAV) für Auspuff
    └── golden/               # Referenzdaten aus lab/ für C++-Tests
```

---

## 5. Datenmodelle

### 5.1 ECU-Tabelle (JSON)

Angelehnt an TunerStudio (Achsen-Bins plus Wertefeld), damit später ein .msq-Import einfach ist.

```json
{
  "id": "ve",
  "name": "Volumetric Efficiency",
  "unit": "%",
  "xAxis": { "name": "rpm", "unit": "1/min", "bins": [800, 1200, ...] },
  "yAxis": { "name": "map", "unit": "kPa",  "bins": [20, 30, ...] },
  "values": [[...], [...]],
  "min": 0, "max": 150
}
```

`values[y][x]`. Bins streng monoton steigend. Interpolation bilinear, an den Rändern geklemmt (`research.md` §2.3).

**Tabellen der ECU (Endausbau):**

| id | Achsen | Einheit | ab Iteration |
|---|---|---|---|
| `ve` | rpm × MAP | % | 2 |
| `lambdaTarget` | rpm × MAP | λ | 2 |
| `ignition` | rpm × MAP | °v. OT | 2 |
| `ignitionMbt` (Modell, nicht ECU) | rpm × MAP | °v. OT | 2 |
| `boostTarget` | rpm × Gaspedal | kPa abs | 8 |
| `wastegateDuty` (Open Loop) | rpm × Boost-Ziel | % | 8 |
| `iatIgnitionCorrection` | IAT | ° | 8 |
| `launchRpm`, `flatShiftCut`, `overrunPops` | 1D/Skalar | – | 10 |

### 5.2 Motordefinition (JSON)

`shared/engines/<id>.json`: Name, Zylinderzahl, Layout (`inline`, `v`, `boxer`), Bankwinkel, Bohrung, Hub, Pleuellänge, Verdichtung, Zündfolge, Zündwinkel-Abstände (Grad KW je Zylinder), Steuerzeiten (EVO/EVC/IVO/IVC), Drehzahlgrenze, Trägheitsmoment Kurbeltrieb + Schwungrad, Saugrohrvolumen, Drosselklappendurchmesser, Auspuffgeometrie (Rohrlängen und -durchmesser je Zylinder, Sammler, Endrohr, Schalldämpfer-IR), optional Lader. Alle Werte, die geschätzt sind, mit `"_assumption": "..."` kommentiert.

**Presets (Endausbau):** `i4_20_na`, `i4_20_turbo`, `i6_30_na` (Referenz), `i6_30_turbo`, `v8_50_crossplane_na`, `v8_50_flatplane_na`, `v8_62_supercharged`, `boxer4_20_turbo` (ungleiche Krümmer).

### 5.3 Zustand (`EngineState`)

`t_s, rpm, throttle, pedal, map_kPa, boost_kPa, iat_C, lambda, lambdaTarget, ignition_deg, knockIndex, knockRetard_deg, torque_Nm, power_kW, airflow_kg_s, fuelflow_kg_s, egt_C, turboRpm, wastegateDuty, bovOpen, superchargerRpm, gear, clutch, vehicleSpeed_kmh, wheelTorque_Nm, dynoMode, cutActive`.

---

## 6. Schnittstelle simcore ↔ UI (Protokoll v1)

Details und Beispiele kommen in Iteration 4 nach `docs/interface.md`. Grundsätze:

- WebSocket, nur `127.0.0.1`, Port 47800 (konfigurierbar).
- Hülle: `{ "v": 1, "type": "<typ>", "id": "<optional, für Antworten>", "payload": { ... } }`
- **simcore → UI:** `hello` (Version, Motor, Tabellenliste), `state` (60 Hz), `dynoProgress`, `dynoResult` (Arrays rpm/torque/power/lambda/boost/afr), `table`, `ack`, `error`.
- **UI → simcore:** `setPedal`, `setDynoMode` (`free` | `holdRpm` | `sweep`), `startSweep {rpmFrom, rpmTo, rate_rpm_s, pedal}`, `stopSweep`, `getTable {id}`, `setTableCells {id, cells:[{x,y,value}]}`, `setTable`, `loadEngine {id}`, `saveTune`, `loadTune`, `setAudio {volume, mute}`, später `shift {gear}`, `setClutch`.
- JSON zuerst (im Browser debugbar). Binärformat (MessagePack) nur, wenn messbar nötig.
- Die UI hat einen **Fake-Modus** (lokaler Datengenerator mit derselben Schnittstelle), damit sie ohne simcore entwickelt werden kann.

---

## 7. Implementierungsansatz je Bereich

Kurz, was gebaut wird. Formeln: siehe `research.md`.

**Tabellen (`domain/ecu/Table1D`, `Table2D`):** Werttyp, unveränderliche Achsen, binäre Suche nach dem Bin, bilineare Interpolation, Klemmung. Aus JSON ladbar. Gründlich getestet (exakt an Stützstellen, Mitte, Ränder, Monotonie).

**Motor-MVM (`domain/engine`):** Saugrohr-Füllmodell (Drosselklappe als Blende, `research.md` §2.5), Luftmasse aus VE × Dichte × Hubraum, Kraftstoff aus Lambda, indiziertes Moment mit Zündwirkungsgrad und Lambda-Faktor, Reibung und Pumpverluste, Drehdynamik. Abgastemperatur grob (für Turbo und Sound).

**ECU (`domain/ecu`):** Speed-Density-Logik wie bei Speeduino/rusEFI (nur Konzepte!): Last = MAP, liest VE/Lambda/Zündung, berechnet Einspritzmenge, Drehzahlbegrenzer (Zünd- oder Kraftstoffunterbrechung), Schubabschaltung. Später Boost-Regelung (Open Loop + PID), Klopfregelung, Launch/Flat Shift/Pops.

**Dyno (`domain/dyno`):** Motorprüfstand als Bremse mit PID-Regler auf Solldrehzahl (`holdRpm`) bzw. Rampe (`sweep`). Messwerte gefiltert, Ergebnis als Kurve. Später Rollenprüfstand mit Rollenträgheit (Iteration 10).

**Akustik (`acoustic/`):** siehe `research.md` §6. Kurbelwinkel im Audio-Takt integrieren, an jedem Auslass-Öffnen einen bandbegrenzten Impuls mit Amplitude aus Zylinderdruck/Last erzeugen, pro Krümmerrohr eine Verzögerungsleitung mit Reflexion und Verlusten, Sammler, Endrohr, Faltung mit Impulsantwort, leichte Zyklusschwankungen, Ansaugrauschen, später Mechanik-Anteil, Lader-Töne, BOV, Fehlzündungen.

**Aufladung (`domain/boost`):** Turbo: Verdichter- und Turbinenkennfeld (zuerst analytisch), Wellendynamik, Wastegate, Ladeluftkühler, BOV, Surge-Erkennung. Kompressor: Roots/Schraube (Verdränger) und Zentrifugal, mechanisch angetrieben, Antriebsleistung vom Kurbelmoment abgezogen, Bypass-Ventil.

**Antrieb (`domain/drivetrain`):** Getriebe (Übersetzungen), Achse, Kupplung (Zustandsautomat haftend/gleitend/offen), Fahrzeug (Masse, Rad, Luft- und Rollwiderstand).

**UI:** Layout «Cockpit» (Gauges oben, Tabelleneditor links, Dyno-Graph rechts, Steuerleiste unten). Dunkles Motorsport-Design, einheitliche Farbskala für Heatmaps. Tabelleneditor: Mehrfachauswahl, +/− Schritte, Bereich setzen, glätten, Interpolieren zwischen Ecken, Rückgängig, Live-Cursor (aktuelle Zelle des Motors), Vergleich mit gespeichertem Tune.

---

## 8. Iterationen

Status: ⬜ offen · 🟨 in Arbeit · ✅ fertig. Claude aktualisiert die Spalte am Ende jeder Iteration.

| # | Iteration | Prio | Neue Disziplin | Status |
|---|---|---|---|---|
| 0 | Repo-Bootstrap | P1 | Git/CMake-Grundlagen | ✅ |
| 1 | Toolchain und Skelette | P1 | Testframeworks | ✅ |
| 2 | Python-MVM und Dyno | P1 | Numerik/Physik | ✅ |
| 3 | Sound-Spike in C++ | P1 | Audio-DSP | ✅ |
| 4 | simcore: MVM-Port, Tabellen, WebSocket | P1 | modernes C++ | ✅ |
| 5 | UI: Cockpit mit Fake-Daten | P2 | React | ⬜ |
| 6 | UI ↔ simcore live | P2 | Integration | ⬜ |
| 7 | Sound an simcore koppeln | P1 | Echtzeit-Threads | ⬜ |
| 8 | Turbo + Klopfen + Turbo-Sound | P2 | Regelungstechnik | ⬜ |
| 9 | Kompressor/Supercharger | P2 | – | ⬜ |
| 10 | Antriebsstrang, Rollenprüfstand, Launch/Flat Shift/Pops | P3 | – | ⬜ |
| 11 | Zylinderaufgelöstes 0D-Modell (Sound v2) | P1 | Thermodynamik | ⬜ |
| 12 | Tauri-Desktop-App | P3 | Tauri/Rust-Basis | ⬜ |
| 13 | 3D-Kennfeld, 3D-Auto, .msq-Import, Presets | P4 | three.js | ⬜ |

### Iteration 0 – Repo-Bootstrap
**Ziel:** Repo mit Struktur und Dokumenten existiert lokal und auf GitHub, CLion kann das Root öffnen.
- Systemprüfung (git, gh, cmake, ninja, Compiler), Ergebnis in `docs/setup.md`.
- Ordnerstruktur aus §4 (leere Ordner mit `.gitkeep`).
- `.gitignore`, `.editorconfig`, `.clang-format`, `README.md`, `THIRD_PARTY_NOTICES.md` (Vorlage).
- Root-`CMakeLists.txt` + `CMakePresets.json` (debug/release, Ninja), `core/CMakeLists.txt` mit einem minimalen Programm `simcore` (gibt Version aus). Keine Abhängigkeiten.
- `git init -b main`, erster Commit, GitHub-Repo anlegen, Push. (Umgesetzt: öffentlich auf Wunsch des Nutzers, github.com/dev-mca/tuning-sim)
**Abnahme:** `cmake --preset debug && cmake --build --preset debug` läuft (falls Tools vorhanden); Repo auf GitHub sichtbar; CLion lädt das Projekt ohne Fehler.

### Iteration 1 – Toolchain und Skelette
- C++: Testframework per FetchContent (ADR: doctest vs. Catch2), ein Beispieltest, `ctest`-Preset. Warnungen hoch, clang-format.
- Python: `lab/.venv`, `pyproject.toml` (numpy, matplotlib, pytest, ruff), Beispieltest.
- UI: Vite + React + TS (strict) + Tailwind + ESLint/Prettier, Startseite «tuning-sim».
- Build-/Test-Befehle in `CLAUDE.md` eintragen.
- Optional: GitHub Actions CI für alle drei Teile (nur nach Rückfrage).
**Abnahme:** `ctest`, `pytest`, `npm run build` und `npm run dev` grün bzw. lauffähig.
**Umgesetzt (2026-11-18):** doctest 2.5.3 (ADR 0002), `lab/` mit hatchling + ruff + pytest, UI mit Vite 8 / React 19 / TS 6 strict / Tailwind v4 / ESLint `strictTypeChecked` / Prettier. CI per GitHub Actions eingerichtet (vom Nutzer bestätigt). Versionen in `docs/setup.md`.

### Iteration 2 – Python-MVM und Dyno (Referenzmotor `i6_30_na`)
- `shared/engines/i6_30_na.json` (2,998 l, Bohrung 84 mm, Hub 90 mm, ε ≈ 10,2, Zündfolge 1-5-3-6-2-4, Drehzahlgrenze 7000/min; Annahmen markiert).
- Tabellen `ve`, `lambdaTarget`, `ignition`, `ignitionMbt` als JSON (16 × 16), plausible Startwerte.
- `Table2D` in Python, Saugrohrmodell, MVM, Reibung, Drehdynamik, Dyno (`holdRpm`, `sweep`).
- Plots: Moment und Leistung über Drehzahl; Vergleich Serie vs. Zündung −5° vs. +3°; Lambda 0,85 vs. 1,0.
- Golden File `shared/golden/i6_30_na_sweep.csv` (mit Einheiten im Kopf).
- Tests: Interpolation, Einheiten, Grössenordnungen (Spitzenmoment ca. 280–330 Nm, Spitzenleistung ca. 190–230 kW, BMEP ≤ 13,5 bar).
**Abnahme:** Kurven plausibel, Effekt von Zündung und Lambda sichtbar, Tests grün.
**Umgesetzt (2026-11-18):** 304 Nm / 196 kW / bmep 12,8 bar / EGT ≈ 785 °C. Zündung −5°: −3 % Moment, EGT +25 °C; λ 1,0 statt 0,88: −4 %; ECU-VE −10 % → λ gemessen 0,98. Zusätzlich zum Plan: Nebenaggregate-Moment (T_aux) und Leerlauf-Momentenreserve in der Zündtabelle, damit der Motor frei bei ~765/min / 30 kPa läuft. Hubraum aus Geometrie 2,993 l. 61 Tests. Plots per `python -m tuning_lab.report`.

### Iteration 3 – Sound-Spike in C++ (`core/spikes/sound_spike`)
- miniaudio per FetchContent, 48 kHz.
- Kurbelwinkelintegration im Audio-Callback, Ereignisse pro Zylinder nach Zündfolge/-abständen, bandbegrenzte Impulse, Wellenleiter pro Krümmerrohr, Sammler, Endrohr, Tiefpass, einfache Faltung (kurze synthetische IR), Zyklusschwankungen.
- Konsole: Drehzahl ±, Last 0–1, Layout I4 / I6 / V8 crossplane / V8 flatplane, WAV-Export 10 s.
- ADR zum Sound-Ansatz.
**Abnahme:** I4, I6, V8 crossplane klar unterscheidbar; glitchfreie Drehzahländerung; WAV-Dateien zum Vergleich.
**Umgesetzt (2026-11-18):** ADR 0003. Unterscheidbarkeit objektiv per Ordnungsanalyse getestet (I4: 98 % auf Ordnung 2; I6: Ordnung 3 dominiert; V8-Bank flatplane 96 % auf geraden Ordnungen, crossplane nur 60 % → halbe Ordnungen). Glitch-Tests für Hochdrehen, Gas weg und Layoutwechsel (80-ms-Überblendung). V8 mit getrennten Bänken in Stereo. WAVs per `sound_spike --export <dir>` (nicht im Repo, da generiert). Release rendert ~50× Echtzeit. Live-Wiedergabe über das Audiogerät vom Nutzer noch anzuhören.

### Iteration 4 – simcore: MVM-Port, Tabellen, WebSocket
- `Table1D/2D`, Motor-MVM, ECU-Grundlogik, Dyno in C++ (domain/), sauber hinter Ports.
- Golden-File-Test gegen Iteration 2 (Toleranz z. B. ±1 %).
- Simulationsschleife mit festem Zeitschritt, Echtzeit- und Schnellmodus.
- CLI: `simcore --engine i6_30_na --sweep 1000 7000` gibt CSV aus.
- WebSocket-Server (ADR zur Bibliothek), Protokoll v1 in `docs/interface.md`, `state` mit 60 Hz, Befehle `setPedal`, `startSweep`, `getTable`, `setTableCells`.
**Abnahme:** Golden-Test grün; mit `wscat` oder einer kleinen HTML-Testseite sichtbar, dass `state` kommt und Befehle wirken.
**Umgesetzt (2026-11-18):** Golden-Test exakt (Toleranz 1e-6 statt ±1 %). ADR 0004 (IXWebSocket). Protokoll v1 in `docs/interface.md` mit `setPedal`, `startSweep`, `stopSweep`, `getTable`, `setTableCells`; `hello`, `state` (60 Hz), `dynoProgress`, `dynoResult`, `table`, `ack`, `error`. Eigene SPSC-Queue + Triple Buffer zwischen Netzwerk- und Simulationsthread. Testseite `core/tools/ws_test.html` und automatischer End-to-End-Test (echter WebSocket-Client). Table1D implementiert, aber noch ungenutzt (ab Iteration 8). 67 C++-Tests.

### Iteration 5 – UI: Cockpit mit Fake-Daten
- Layout, Designsystem (Farben, Schrift, Heatmap-Skala).
- Gauges: Drehzahl (mit Begrenzer-Zone), Boost/MAP, Lambda/AFR, IAT, Zündwinkel. Nadel-Glättung mit `requestAnimationFrame`.
- Tabelleneditor (Canvas-Heatmap) mit Auswahl, ±, Glätten, Rückgängig, Live-Cursor.
- Dyno-Graph mit uPlot, Overlay mehrerer Läufe.
- Fake-Datenquelle mit derselben Schnittstelle wie der WebSocket-Client.
**Abnahme:** Cockpit sieht gut aus, alles bewegt sich flüssig, Tabellen editierbar.

### Iteration 6 – UI ↔ simcore live
- WebSocket-Client mit Reconnect, Umschalten Fake/Live.
- Tabellenänderung → simcore → neuer Sweep → neue Kurve im Overlay.
- Tune speichern/laden (JSON).
**Abnahme:** Zelle in der Zündtabelle ändern, Sweep starten, Unterschied in der Kurve sehen.

### Iteration 7 – Sound an simcore koppeln
- Akustik aus dem Spike als `acoustic/`-Modul in simcore, Parameter aus der Simulation (`AcousticParams`).
- Amplitude und Klangfarbe aus Last, Zündwinkel, Lambda; Drehzahlbegrenzer und Schubabschaltung hörbar.
- Lautstärke/Mute über UI.
**Abnahme:** Vollgas-Sweep tönt glaubwürdig; Spätzündung tönt hörbar matter; kein Knacksen bei Lastwechseln.

### Iteration 8 – Turbo, Klopfen, Turbo-Sound
- Verdichter-/Turbinenmodell, Wellendynamik, Wastegate (Open Loop + PID auf `boostTarget`), Ladeluftkühler, Abgasgegendruck.
- Klopfmodell (Livengood-Wu mit Douaud-Eyzat) und Klopfregelung (Zündrücknahme).
- BOV und Surge-Erkennung.
- Sound: Verdichterpfeifen, BOV-Abblasen, Verdichterpumpen ohne BOV, gedämpfter Auspuffklang hinter der Turbine.
- Presets `i6_30_turbo`, `i4_20_turbo`; Gauges Turbo-Drehzahl, Wastegate-Duty.
**Abnahme:** Turboloch sichtbar und hörbar; schlecht eingestellter PID schwingt; zu viel Frühzündung unter Boost führt zu Klopfen und Leistungsverlust.

### Iteration 9 – Kompressor/Supercharger
- Verdrängerlader (Roots/Schraube) und Zentrifugalkompressor, Riemenübersetzung, Antriebsleistung, Bypass.
- Sound: Laderheulen proportional zur Laderdrehzahl.
- Preset `v8_62_supercharged`; Dyno-Vergleich Turbo vs. Kompressor.
**Abnahme:** Kompressor liefert Ladedruck ohne Verzögerung, kostet sichtbar Leistung, heult hörbar.

### Iteration 10 – Antriebsstrang, Rollenprüfstand, Tuning-Funktionen
- Getriebe, Achse, Kupplung, Fahrzeugmodell, Rollenprüfstand-Modus, Schalten per Tastatur/UI.
- ECU: Launch Control, Flat Shift (Zündunterbrechung), Pops & Bangs im Schub.
- Sound: Drehzahlabfall beim Schalten, Fehlzündungen.
**Abnahme:** Durchzug im 4. Gang auf der Rolle; Launch und Flat Shift hörbar.

### Iteration 11 – Zylinderaufgelöstes 0D-Modell (Sound v2)
- Pro Zylinder: Volumen über Kurbelwinkel, Wiebe-Brennverlauf, Druck/Temperatur, Ventilströmung, Auslassmassenstrom als Schallquelle.
- Optional 1D-Auspuff. Schritt ≤ 2° KW bei Drehzahlgrenze.
- Vergleich MVM vs. 0D (Drehmoment) im Lab.
**Abnahme:** Krümmerlänge verändert hörbar Klang und messbar die Drehmomentkurve.

### Iteration 12 – Tauri-Desktop-App
- Tauri v2, simcore als Sidecar (`bundle.externalBin`), sauberes Starten/Beenden.
**Abnahme:** installierbares Paket für mein OS.

### Iteration 13 – Extras
- 3D-Kennfeld (react-three-fiber), einfaches 3D-Auto, .msq-Import, weitere Presets.

---

## 9. Risiken und Gegenmassnahmen

| Risiko | Gegenmassnahme |
|---|---|
| Sound enttäuscht | Früher Spike (Iteration 3); Referenz-WAVs; Ansatz in Iteration 11 vertiefen; optional `EngineSimSoundAdapter` hinter demselben Port (MIT, Lizenzhinweis) |
| Zu viele neue Technologien | Höchstens eine neue Disziplin pro Iteration |
| Audio knackt | Strikte Thread-Regeln, Parameter-Interpolation, Puffergrösse konfigurierbar |
| Physik unplausibel | Plausibilitätstests mit Grössenordnungen, Golden Files, Vergleich mit Seriendaten |
| Motivation sinkt | Jede Iteration liefert etwas Sicht- oder Hörbares |
| Lizenzprobleme | Nur MIT/BSD/Zlib/Public-Domain-Code übernehmen, GPL nur als Ideenquelle |

---

## 10. Backlog (Ideen ohne Termin)

- Software-in-the-Loop mit dem rusEFI-Simulator als echter ECU
- Ethanol/E85 mit eigenem Lambda-Stöchiometrie-Wert
- Nockenwellenverstellung (VANOS) als Tabelle
- Ansaugsound, Ventiltrieb- und Steuerkettengeräusche
- Logging und Log-Viewer (wie MegaLogViewer)
- Auto-Tune der VE-Tabelle aus simulierten Lambda-Messungen
- WebAssembly-Web-Demo
- Realistisches Abgassystem implementieren (von Krümmer bis Enddtop alle Komponenten)
- Neue Werte wie Katalysatortemparatur (Motor dreht beim Kaltstart etwas höher beim idle, um den Kat schnell warm zu bekommen, darum hört sich das auto cooler an)
- Schubabschaltung abstellen und dann sollte es beim warmen Kat knallen, burblen usw. 
