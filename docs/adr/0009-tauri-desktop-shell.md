# ADR 0009 – Tauri-Desktop-Hülle mit simcore als Sidecar

- Status: angenommen (2026-11-28, vorher vorgeschlagen)
- Nachtrag beim Merge: Offene Punkte (Port zur Laufzeit, Name des Sidecars, CI-Job, AppImage) stehen im Backlog von plan.md.
- Datum: 2026-11-28

## Kontext

Iteration 12 verpackt UI und simcore als Desktop-App (plan.md §8). Entschieden sind schon
Tauri v2, simcore als Sidecar (`bundle.externalBin`) und Rust nur für die Hülle, ohne FFI
(plan.md §3, ADR 0001). Offen war: Wie kommt das CMake-Binary in den Tauri-Build, wie startet
und beendet die Hülle simcore, woran erkennt sie, dass simcore bereit ist, woher kommen die
Motordaten, und was passiert mit dem Port 47800, auf dem oft schon ein eigenes simcore läuft.
Abnahme: installierbares Paket für Fedora.

## Optionen

### Sidecar bereitstellen (Tauri erwartet `binaries/simcore-<target-triple>`)

| Option | Vorteil | Nachteil |
|---|---|---|
| **Skript `app/scripts/stage-sidecar.sh` kopiert `build/release/core/simcore`** | `core/` bleibt unverändert, CMake- und Cargo-Build bleiben getrennt | simcore muss vorher mit CMake gebaut sein (Skript sagt das klar) |
| CMake kopiert nach dem Build (`add_custom_command`) | kein Extra-Schritt | `core/` kennt dann Tauri, falsche Abhängigkeitsrichtung |
| `build.rs` ruft CMake auf | ein Befehl für alles | langsam, zwei Build-Systeme ineinander, schlecht in CLion |

### Prozess steuern

| Option | Vorteil | Nachteil |
|---|---|---|
| **`std::process` + `libc` (SIGTERM, `PR_SET_PDEATHSIG`)** | simcore beendet sich über seinen SIGTERM-Handler sauber; stirbt die Hülle (Absturz, `kill`), schickt der Kernel simcore SIGTERM | eigener Code (~150 Zeilen), Waisenschutz nur unter Linux |
| `tauri-plugin-shell` (`sidecar()`) | offizieller Weg, plattformübergreifend | `kill()` schickt SIGKILL (simcore räumt nicht auf), kein Schutz bei Absturz der Hülle, zusätzliche Berechtigungen |

### Bereitschaft prüfen

| Option | Vorteil | Nachteil |
|---|---|---|
| **WebSocket-Handshake (HTTP-Upgrade, 101 erwartet), danach Close-Frame** | prüft genau das, was die UI braucht; simcore loggt nichts | ~40 Zeilen eigener Code |
| reiner TCP-Connect | einfach | IXWebSocket schreibt pro Probe «Error reading HTTP request line» nach stderr; nimmt jeden fremden Listener als bereit |
| Zeile «listening on …» auf stdout abwarten | kein Netzwerkzugriff | hängt an einem Log-Text, der sich ändern kann |

## Entscheid

1. **Neuer Ordner `app/`**: `app/package.json` (nur `@tauri-apps/cli`) und `app/src-tauri/`
   (Cargo). Die UI bleibt in `ui/` und wird nicht verändert. `tauri dev` startet Vite auf
   Port **5180** (`--strictPort`, nicht 5173, dort läuft oft der eigene Dev-Server), `tauri build`
   baut `ui/dist`. Beides erledigt `app/scripts/prepare-ui.sh`, das vorher den Sidecar
   bereitstellt (`beforeDevCommand`/`beforeBuildCommand`).
2. **Sidecar-Skript** wie oben. `SIMCORE_BIN` überschreibt die Quelle (z. B. Debug-Build).
   Der Target-Triple kommt aus `rustc --print host-tuple`.
3. **Prozesssteuerung mit `std::process`** (`app/src-tauri/src/sidecar.rs`):
   - Start im Main-Thread im `setup`-Hook mit `--shared <Ressourcen>/shared --port <p>`.
     `PR_SET_PDEATHSIG` gilt pro Thread, darum der Main-Thread.
   - Beenden bei `RunEvent::Exit`: SIGTERM, bis 3 s warten, dann SIGKILL. `Drop` macht dasselbe.
   - stderr wird weitergereicht, die letzten 20 Zeilen erscheinen in der Fehlermeldung.
4. **Bereitschaft per WebSocket-Handshake** (`probe.rs`), alle 50 ms, Timeout 10 s. Endet
   simcore vorher, kommt der Fehler sofort (mit stderr). Das Fenster startet unsichtbar und
   wird erst nach erfolgreichem Handshake gezeigt. Fehler: nativer Dialog
   (`tauri-plugin-dialog`), Meldung auch auf stderr, Exit-Code 1.
5. **Port**: Vor dem Start prüft die Hülle, ob der Port belegt ist, und bricht dann mit klarer
   Meldung ab. Sonst würde sie ein fremdes simcore (z. B. aus dem Terminal) für das eigene halten.
   `TUNING_SIM_PORT` setzt den Port zur Laufzeit (Standard 47800). Die UI liest die Adresse beim
   **Build** (`VITE_SIMCORE_URL`), `prepare-ui.sh` leitet sie aus `TUNING_SIM_PORT` ab.
   `TUNING_SIM_NO_AUDIO=1` startet simcore mit `--no-audio`.
6. **Motordaten als Ressourcen**: `shared/engines/` und `shared/tables/` (nicht `golden/`).
   Installiert unter `/usr/lib/tuning-sim/shared/`.
7. **Sicherheit**: Die UI braucht keine Tauri-Befehle; Capability nur `core:default`. CSP:
   `default-src 'self'`, `connect-src` erlaubt `ws://127.0.0.1:*`, Styles `'unsafe-inline'`
   (Tailwind/uPlot setzen Inline-Styles). Im Dev-Modus zusätzlich Vite-HMR.
8. **Paket: nur RPM** (Fedora). AppImage lädt beim Build Werkzeuge (linuxdeploy) aus dem Netz,
   `.deb` passt nicht zum System des Nutzers.

## Konsequenzen

- (+) `core/`, `ui/` und das Protokoll bleiben unverändert. simcore bleibt ein eigenständiger
  Prozess, den man weiterhin ohne Tauri starten kann.
- (+) Start, sauberes Beenden und Waisenschutz sind automatisch getestet, auch gegen das echte
  simcore (`app/src-tauri/tests/simcore_sidecar.rs`, ohne Fenster).
- (−) Der Port steckt in der gebauten UI. Ein anderer Port braucht einen neuen Build mit
  demselben `TUNING_SIM_PORT`. Abhilfe wäre, dass die UI die Adresse zur Laufzeit liest
  (z. B. Query-Parameter), das ändert `ui/src`.
- (−) Unter macOS/Windows gibt es kein `PR_SET_PDEATHSIG` und unter Windows kein SIGTERM:
  simcore würde dort nach 3 s hart beendet, und ein Absturz der Hülle liesse simcore laufen.
  Heute nur Linux im Fokus.
- (−) Stürzt simcore während des Betriebs ab, zeigt die UI «getrennt», die Hülle startet simcore
  nicht neu.
- (−) Das RPM installiert simcore als `/usr/bin/simcore` (Tauri legt Sidecars neben das
  Hauptprogramm). Ein generischer Name im System-`PATH`.
- (−) `cargo build` im Ordner `app/src-tauri` braucht einen bereitgestellten Sidecar
  (`npm run stage-sidecar`), sonst bricht `tauri-build` ab.

## Offene Fragen

- Soll die UI die simcore-Adresse zur Laufzeit lesen (z. B. `?simcore=ws://…`), damit der Port
  ohne neuen Build wählbar ist? Ändert `ui/src`.
- CI-Job für `app/` (Rust-Toolchain, WebKitGTK-Pakete, Sidecar aus dem CMake-Job)?
- Sidecar im Paket umbenennen (z. B. `tuning-sim-simcore`), um `/usr/bin/simcore` zu vermeiden?
- Zusätzlich AppImage oder Flatpak für andere Distributionen?
