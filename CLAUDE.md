# CLAUDE.md – tuning-sim

Diese Datei ist der Einstiegspunkt für jede Claude-Code-Session. Lies sie vollständig, bevor du etwas tust.

## Pflichtlektüre zu Beginn jeder Session

1. Diese Datei (`CLAUDE.md`)
2. `docs/plan.md` – Architektur, Tools, Iterationen, Prioritäten, Abnahmekriterien. **Der Plan ist die Quelle der Wahrheit.**
3. `docs/research.md` – Physik, Mathematik und Quellen. Lies die Abschnitte, die zur aktuellen Iteration gehören, bevor du Physik- oder Audio-Code schreibst.
4. Den Abschnitt «Aktueller Stand» unten und `git log --oneline -20`.

Dann sag mir kurz, welche Iteration als nächste ansteht und was du vorhast, und warte auf mein «go».

## Projekt in einem Satz

Desktop-Simulator für Motor-Tuning: ECU-Kennfelder, Turbo/Kompressor, Getriebe und Dyno, physikalisch synthetisierter Motorsound (ein Reihensechser muss wie ein Reihensechser tönen) und eine coole UI mit Gauges, Heatmap-Tabellen und Dyno-Graphen.

## Über mich (den Nutzer)

- Student, Java-Hintergrund (Programmieren 1 und 2, Clean Code, Algorithmen), lerne TypeScript. C++ und Python-Numerik sind neu für mich.
- Ich will **lernen**. Erkläre jede neue Datei, jeden neuen Begriff und jede Entscheidung in 2–4 Sätzen, gern mit Java-Analogie. Keine Romane.
- IDEs: CLion (C++), IntelliJ Ultimate (TypeScript, Python).
- Kommunikation mit mir auf **Deutsch** (Schweizer Rechtschreibung, «ss» statt «ß»).

## Architektur (Kurzfassung, Details in `docs/plan.md`)

- Monorepo, hexagonale Architektur (Ports & Adapters), **kein** Microservice.
- `core/` C++20: deterministischer Simulationskern («simcore»), läuft als eigenständiger Headless-Prozess. Er erzeugt und spielt **auch den Sound** (miniaudio).
- `ui/` TypeScript + React + Vite + Tailwind; spricht mit simcore über **WebSocket auf 127.0.0.1** (JSON, versioniert). Später verpackt Tauri v2 alles, mit simcore als Sidecar.
- `lab/` Python: Rechenlabor, Plots, Golden Files für C++-Tests. Kein zweiter Simulator.
- `shared/` Motordefinitionen, ECU-Tabellen (JSON), Golden Files.
- Audio **nie** über die WebSocket-Schnittstelle schicken.

## Konventionen

### Code
- Bezeichner, Kommentare im Code und Commit-Messages auf **Englisch**.
- Clean Code: sprechende Namen, kleine Funktionen, keine Magic Numbers.
- **SI-Einheiten intern**. Einheiten im Namen, wenn nicht offensichtlich: `displacement_m3`, `pressure_Pa`, `speed_rad_s`. Umrechnung (rpm, bar, PS) nur an den Rändern (UI, Ein-/Ausgabe).
- Kommentare erklären das **Warum**. Jede Physikformel bekommt einen Kommentar mit Verweis auf den Abschnitt in `docs/research.md`.
- C++: C++20, `clang-format` (Stil in `.clang-format`), Warnungen hoch (`-Wall -Wextra -Wpedantic` bzw. `/W4`), keine rohen `new`/`delete`, keine Exceptions im Audio-Thread.
- Audio-Callback: **keine** Allokation, **keine** Locks, **kein** I/O. Nur atomare Parameter oder lock-freie Ringpuffer.
- TypeScript: `strict: true`, ESLint + Prettier.
- Python: Typ-Hints, `ruff` für Lint/Format, pytest.

### Tests
- Jede Logik bekommt Tests. Physikfunktionen bekommen Plausibilitätstests (Einheiten, Grössenordnungen, Grenzfälle).
- C++-Physik wird gegen Golden Files aus `lab/` mit Toleranz geprüft.
- Vor jedem Commit laufen alle Tests der betroffenen Teile.

### Git
- Branch: `main`. Für grössere Iterationen ein Branch `iter/NN-kurzname`, am Ende Merge nach `main`.
- **Conventional Commits** auf Englisch: `feat(core): add Table2D bilinear interpolation`, `fix(ui): ...`, `docs: ...`, `test(lab): ...`, `chore: ...`, `refactor(core): ...`. Scopes: `core`, `ui`, `lab`, `shared`, `docs`, `build`.
- Kleine, in sich geschlossene Commits. Keine generierten Dateien, keine Build-Ordner, keine venvs, kein `node_modules`.
- Nach jeder abgeschlossenen Iteration: Tests grün → Commit → `git push`.

## Arbeitsregeln

1. **Pro Session genau eine Iteration** aus `docs/plan.md` (ausser ich sage etwas anderes).
2. Nichts umsetzen, was nicht zur aktuellen Iteration gehört. Ideen für später kommen in `docs/plan.md` unter «Backlog».
3. Bei mehreren sinnvollen Wegen: 2–3 Optionen mit je einem Satz Vor- und Nachteil zeigen, **mich fragen**, Entscheid als ADR in `docs/adr/NNNN-titel.md` festhalten.
4. Nichts Grosses installieren (Compiler, SDKs, globale Pakete, IDEs) ohne meine Zustimmung. Projektlokale Abhängigkeiten (venv, npm im Projekt, CMake FetchContent) sind ok.
5. Fremden Code nur nach Rückfrage übernehmen. Lizenz prüfen: MIT/BSD/Zlib ok mit Eintrag in `THIRD_PARTY_NOTICES.md`. **GPL-Code (z. B. rusEFI, Speeduino) nicht kopieren**, nur Konzepte übernehmen.
6. Bibliotheksversionen nicht aus dem Gedächtnis setzen. Aktuelle stabile Version ermitteln und in `docs/setup.md` festhalten.
7. Am Ende jeder Iteration:
   - Abnahmekriterien aus `docs/plan.md` prüfen
   - Status in `docs/plan.md` aktualisieren
   - «Aktueller Stand» und ggf. «Learnings» in dieser Datei aktualisieren
   - Commit + Push
   - Zusammenfassung an mich: was gebaut wurde, was ich daraus lernen sollte, wie ich es selbst starte/teste, offene Fragen. Dann **STOPP**.

## Diese Datei pflegen

Du darfst und sollst `CLAUDE.md` selbst erweitern, wenn du etwas lernst, das künftige Sessions wissen müssen: Build-Befehle, Stolpersteine, getroffene Entscheidungen (mit Verweis auf ADR), Eigenheiten meines Systems. Halte sie **kurz** (unter ca. 200 Zeilen). Details gehören in `docs/`. Ändere nie die Abschnitte «Über mich», «Konventionen» und «Arbeitsregeln» ohne meine Zustimmung.

## Build- und Test-Befehle

```bash
# C++ (aus dem Repo-Root), Build-Ordner: build/<preset>/
cmake --preset debug            # konfigurieren (Ninja, Debug); "release" analog
cmake --build --preset debug    # bauen
./build/debug/core/simcore      # Server ws://127.0.0.1:47800 mit Ton (Ctrl+C beendet); --no-audio ohne Ton
./build/release/core/simcore --sound-sweep out.wav [--ignition-offset -10]   # Sweep-Sound als WAV
./build/release/core/simcore --sound-blips out.wav --no-overrun-cut --overrun-retard 35 --overrun-enrichment 0.1   # Pops
./build/release/core/simcore --sound-launch out.wav [--launch-rpm 4000] [--launch-retard 10]   # 2-Step
./build/debug/core/simcore --sweep 1000 7000   # Dyno-Sweep als CSV (Format wie Golden File)
ctest --preset debug            # Tests (doctest, ADR 0002)
find core -name '*.[ch]pp' | xargs clang-format -i   # formatieren
./build/release/core/spikes/sound_spike/sound_spike             # Sound-Spike live (Tasten: README)
./build/release/core/spikes/sound_spike/sound_spike --export wav # Vergleichs-WAVs, ohne Audiogerät

# Python (aus lab/)
python3 -m venv .venv && .venv/bin/pip install -e ".[dev]"   # einmalig
.venv/bin/pytest                                             # Tests
.venv/bin/ruff check . && .venv/bin/ruff format .            # Lint + Format
.venv/bin/python -m tuning_lab.report                        # Dyno-Plots -> lab/output/
.venv/bin/python -m tuning_lab.golden                        # Golden-File neu erzeugen

# UI (aus ui/)
npm ci                          # Abhängigkeiten exakt nach package-lock.json
npm run dev                     # Dev-Server http://localhost:5173
npm test                        # Vitest (Logik, Fake-Quelle, Store)
npm run test:e2e                # End-to-End gegen laufendes simcore (vorher ./build/debug/core/simcore --no-audio)
npm run build                   # Typecheck + Produktions-Build nach dist/
npm run lint && npm run format  # ESLint + Prettier

# Desktop-App (aus app/; PATH="$HOME/.cargo/bin:$PATH", vorher Release-Build von simcore)
npm ci && npm run dev           # Vite :5180 + Fenster; TUNING_SIM_PORT=47801 TUNING_SIM_NO_AUDIO=1 optional
npm run build                   # RPM in src-tauri/target/release/bundle/rpm/
npm run stage-sidecar && (cd src-tauri && cargo test && cargo clippy --all-targets -- -D warnings && cargo fmt --check)

# 0D-Modell (aus lab/)
.venv/bin/python -m tuning_lab.zero_d.report                 # 0D vs MVM, Plots -> lab/output/zero_d/
```

CI: `.github/workflows/ci.yml` führt bei jedem Push auf `main` und `iter/**` alle drei Teile aus (inkl. Format-Checks). Vor dem Push lokal dieselben Befehle laufen lassen.

## Aktueller Stand

- Letzte Arbeit: **Welle 1 (2026-11-28)** mit 5 parallelen Agents gemerged: Origin-Check + Achsen-Rundung (core), 0D-Prototyp (lab, Iteration 11 🟨), Tauri-Hülle `app/` (12 🟨), 3D-Kennfeld + .msq-Import (13 🟨). Details plan.md §8.
- Iteration **7c – 2-Step im Stand ✅** (2026-11-28, ADR 0014).
- Iteration **10a – Protokoll-Vertrag ✅** (2026-11-28).
- Nächste Iteration: **10 Antriebsstrang**, danach 8 Turbo.
- System des Nutzers: Fedora Linux 43 (KDE), GCC 15.3.1, CMake 3.31.11, Ninja 1.13.1, clang-format 21.1.8, Python 3.14.10, Node 22.23.5/npm 10.9.9, git 2.56.1, gh 2.87.3 (kein clang++). Details und Bibliotheksversionen: `docs/setup.md`
- GitHub: https://github.com/dev-mca/tuning-sim (**öffentlich**), Remote via SSH, Branch `main`
- ADRs: 0001 Architektur, 0002 doctest, 0003 Sound-Ansatz (Impulsmodell + Wellenleiter), 0004 IXWebSocket, 0005 UI-Zustand/Rendering, 0006 Live-Verbindung/Tune-Dateien, 0007 Sound in simcore, 0008 Pops & Bangs, 0009 Tauri-Hülle, 0010 0D-Modell (Lab), 0011 Origin-Check, 0012 .msq-Import, 0013 3D-Kennfeld, 0014 2-Step
- simcore (`core/src/`): `domain/` (Table1D/2D, Saugrohr, MVM inkl. Nachverbrennung, ECU mit Begrenzer + Schubabschaltung + Schub-Einstellungen, Kat-Temperatur, Dyno/SweepRun; bitgenau wie lab, Golden Files Sweep + Overrun), `acoustic/` + `dsp/` (Bibliothek `simcore_acoustic`: Pulse, Wellenleiter, Nachverbrennungs-Knaller, `AcousticParams` → `SoundTarget`), `ports/` (Interfaces inkl. `IAcousticSink`), `adapters/` (JSON-Repository, Protokoll-Codec, MessageBus, WebSocket-Server, CSV, WAV, miniaudio `AudioOutput`), `app/` (SimulationService, RealtimeRunner, `SoundRenderer`, `engine_acoustics`, `sound_sweep`, CLI, main), `concurrency/` (SPSC-Queue, Triple Buffer). Protokoll v1: `docs/interface.md`.
- UI-Cockpit (`ui/src/`, Übersicht in `ui/README.md`): Port `sim/SimSource.ts`, `sim/fake/FakeSimSource` (Protokoll v1, Daten aus `shared/`), `state/simStore.ts`, Gauges (SVG), Tabelleneditor (Canvas), Dyno (uPlot). `App.tsx` wählt die Quelle (Live = `sim/live/WebSocketSimSource`, Fake), Tunes als JSON in der UI (`domain/tuneFile.ts`).
- Lab (Referenz für den C++-Port in Iteration 4): MVM, ECU, Dyno in `lab/src/tuning_lab/` (Übersicht in `lab/README.md`); Referenzmotor `shared/engines/i6_30_na.json`, Tabellen `shared/tables/i6_30_na/`, Golden File `shared/golden/i6_30_na_sweep.csv`. Ergebnis Serie: 304 Nm, 196 kW, bmep 12,8 bar, Leerlauf 765/min bei 30 kPa.
- Desktop-App `app/` (Tauri 2, ADR 0009): Rust-Hülle startet simcore als Sidecar, lädt `ui/`; Setup in `docs/setup.md`.
- 0D-Prototyp `lab/src/tuning_lab/zero_d/` (ADR 0010): nur Lab, ändert weder MVM noch Golden Files.
- Sound-Spike `core/spikes/sound_spike/` (README dort): nur noch interaktives Vergleichs-Frontend (I4/I6/V8), nutzt `simcore_acoustic`.

## Learnings

- 2026-11-18: CLion nutzt System-CMake und nur das Preset-Profil «debug» (gleicher Build-Ordner wie im Terminal), siehe `docs/setup.md`.
- 2026-11-18: TypeScript bleibt auf `~6.0.x`, solange typescript-eslint TS 7 nicht unterstützt (Peer-Dependency prüfen, bevor man TS hebt).
- 2026-11-18: create-vite nutzt standardmässig Oxlint. Wir nutzen ESLint (plan.md), daher Flag `--eslint`.
- 2026-11-18: In `core/` neue Quelldateien zu `simcore_lib` hinzufügen (nicht zu `simcore`), damit die Tests sie linken können.
- 2026-11-18: Saugrohr ist nahe Vollgas steif (Psi steil bei Pi→1). Lösung: Psi linear für Pi > 0,99 und 10 Teilschritte pro 1 ms. Der C++-Port muss das identisch machen, sonst passt das Golden File nicht.
- 2026-11-18: Leerlauf braucht Momentenreserve (Zündung ~4°), tiefe VE bei Unterdruck und T_aux; sonst hängt der MAP bei 16 kPa. Ein Bremsprüfstand kann den Motor nicht antreiben (Bremse ≥ 0), Leerlauf daher frei laufend testen.
- 2026-11-18: Hubraum aus Bohrung/Hub = 2,993 l (plan.md nennt 2,998 l). Kurbelwinkel intern in Grad (`_deg`), sonst SI.
- 2026-11-18: Abhängigkeiten zentral in `core/cmake/Dependencies.cmake` (Tarball + SHA256). miniaudio als eigenes C-Target ohne Decoder/Engine; nur `adapters/audio_output.cpp` (und der Spike-Adapter) inkludieren `miniaudio.h`.
- 2026-11-18: Sound objektiv prüfen statt nur hören: Goertzel-Ordnungsanalyse in `core/tests/acoustic/order_analysis.hpp` (Analysedauer = ganze Zahl von Arbeitsspielen, sonst Leckage). Spektrogramme lassen sich mit dem Lab-venv aus den WAVs erzeugen.
- 2026-11-18: `.clang-format` mit `AccessModifierOffset: -4` (public:/private: am Zeilenanfang).
- 2026-11-18: C++-Port gibt das Golden File exakt wieder (0 Abweichung bei 6 Dezimalen), weil jede Formel in derselben Gleitkomma-Reihenfolge wie Python steht. Bei Physikänderungen: zuerst lab ändern, Golden neu erzeugen, dann C++ identisch nachziehen.
- 2026-11-18: Domain-Code kennt kein JSON/Netzwerk: Einheiten-Umrechnung nur in `adapters/table_units` und im Protokoll-Codec. Neue Befehle: Struct in `ports/messages.hpp`, Decoder im Codec, Handler in `SimulationService`, Doku in `docs/interface.md`.
- 2026-11-19: UI: 60-Hz-Daten nie in React-State, sondern `store.latestState()` per `useAnimationFrame` lesen. Event-Handler, die Tabellen ändern, nutzen `store.editTableWith(...)` und eine Ref für die Auswahl, sonst arbeiten schnelle Tastenfolgen mit einem veralteten Stand (stale closure).
- 2026-11-19: Vitest-Fake-Timer: Ein `setTimeout(0)`, das in einem anderen Timer geplant wird, läuft erst nach `advanceTimersByTime(1)`.
- 2026-11-19: UI-Screenshots ohne Chrome: `firefox --headless --screenshot` wartet auf `load`. Für spätere Zustände eine temporäre Seite mit iframe und einem verzögerten Bild nutzen (nicht committen).
- 2026-11-19: Live-Daten unterscheiden sich vom Fake: simcore meldet gemessene Dyno-Drehzahlen (nicht ganzzahlig) und Achsen mit Gleitkomma-Rauschen (1100,0000000000002). Anzeige über `formatClean`, Achsenvergleich mit Toleranz, Läufe fürs Overlay interpolieren. Neue UI-Features immer auch live prüfen.
- 2026-11-19: Commit-Subject max. 72 Zeichen (Hook des engineering-fundamentals-Plugins blockiert sonst).
- 2026-11-22: Klangfarbe wird durch die Pulsform bestimmt, nicht durch den Tiefpass bei einigen kHz (hörbare Energie < 1,5 kHz). Helligkeit messen: RMS der ersten Differenz / RMS. Sweep-WAVs mit `--sound-sweep` erzeugen und vergleichen.
- 2026-11-22: Nach `git stash` zum Prüfen eines Zwischen-Commits neu bauen, sonst laufen veraltete Binaries (z. B. «unknown option»).
- 2026-11-22: Der Nutzer hat oft eigene `simcore`/`npm run dev` laufen (Ports 47800/5173). Für eigene Tests andere Ports nehmen (`--port 47801`, `VITE_SIMCORE_URL=ws://127.0.0.1:47801 npx vite --port 5174`), nie seine Prozesse beenden.
- 2026-11-22: Der Standard-Audioausgang des Nutzers ist HDMI (GA104). Kommt kein Ton, das Standardgerät in den KDE-Einstellungen prüfen.
- 2026-11-28: Rust liegt in `~/.cargo/bin` (rustup, nicht im PATH). `cargo build` in `app/src-tauri` scheitert ohne gestagten Sidecar (`npm run stage-sidecar`). simcore nie per nacktem TCP-Connect prüfen (IXWebSocket loggt, SIGPIPE), immer echter Handshake.
- 2026-11-28: Parallele Agents in Worktrees: eigenes `lab/.venv` je Worktree (das venv im Haupt-Checkout zeigt auf den Haupt-Baum), eigene Ports je Agent, feste ADR-Nummern vorab verteilen, `ui/package-lock.json` beim Merge mit `git checkout --ours` + `npm install` neu erzeugen. Worktrees liegen in `.claude/worktrees/` (nicht committen).
- 2026-11-28: Protokolländerungen beginnen beim Beispiel in `shared/protocol/v1/` (Vertragstests in core und ui, `docs/interface.md` «Vertrag»). Im TS-Vertragstest steht `Required<EngineState>`, deshalb meldet der Typecheck fehlende Felder.
- 2026-11-28: Blockiert ein Hook einen Bash-Befehl, läuft **nichts** davon, auch Dateiänderungen davor nicht. Danach prüfen, ob die Änderung wirklich drin ist.
- 2026-11-18: GitHub blockiert Pushes mit privater Commit-Mail (GH007). In diesem Repo ist lokal `user.email` = GitHub-Noreply-Adresse gesetzt. Nicht ändern, das Repo ist öffentlich.
