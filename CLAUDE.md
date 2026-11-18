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
./build/debug/core/simcore      # starten, gibt Name + Version aus
ctest --preset debug            # Tests (doctest, ADR 0002)
find core -name '*.[ch]pp' | xargs clang-format -i   # formatieren

# Python (aus lab/)
python3 -m venv .venv && .venv/bin/pip install -e ".[dev]"   # einmalig
.venv/bin/pytest                                             # Tests
.venv/bin/ruff check . && .venv/bin/ruff format .            # Lint + Format

# UI (aus ui/)
npm ci                          # Abhängigkeiten exakt nach package-lock.json
npm run dev                     # Dev-Server http://localhost:5173
npm run build                   # Typecheck + Produktions-Build nach dist/
npm run lint && npm run format  # ESLint + Prettier
```

CI: `.github/workflows/ci.yml` führt bei jedem Push auf `main` und `iter/**` alle drei Teile aus (inkl. Format-Checks). Vor dem Push lokal dieselben Befehle laufen lassen.

## Aktueller Stand

- Iteration: **1 – Toolchain und Skelette ✅ abgeschlossen** (2026-11-18)
- Nächste Iteration: **2 – Python-MVM und Dyno**
- System des Nutzers: Fedora Linux 43 (KDE), GCC 15.3.1, CMake 3.31.11, Ninja 1.13.1, clang-format 21.1.8, Python 3.14.10, Node 22.23.5/npm 10.9.9, git 2.56.1, gh 2.87.3 (kein clang++). Details und Bibliotheksversionen: `docs/setup.md`
- GitHub: https://github.com/dev-mca/tuning-sim (**öffentlich**), Remote via SSH, Branch `main`
- ADRs: 0001 Architektur, 0002 doctest
- Bereits vorhanden: `simcore_lib` (statische Lib, alle Logik dort), `simcore_warnings`; `tuning_lab.units` (SI-Umrechnungen); UI-Startseite mit ersten Design-Tokens (`ui/src/index.css`, `@theme`)

## Learnings

- 2026-11-18: CLion nutzt System-CMake und nur das Preset-Profil «debug» (gleicher Build-Ordner wie im Terminal), siehe `docs/setup.md`.
- 2026-11-18: TypeScript bleibt auf `~6.0.x`, solange typescript-eslint TS 7 nicht unterstützt (Peer-Dependency prüfen, bevor man TS hebt).
- 2026-11-18: create-vite nutzt standardmässig Oxlint. Wir nutzen ESLint (plan.md), daher Flag `--eslint`.
- 2026-11-18: In `core/` neue Quelldateien zu `simcore_lib` hinzufügen (nicht zu `simcore`), damit die Tests sie linken können.
- 2026-11-18: GitHub blockiert Pushes mit privater Commit-Mail (GH007). In diesem Repo ist lokal `user.email` = GitHub-Noreply-Adresse gesetzt. Nicht ändern, das Repo ist öffentlich.
