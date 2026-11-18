# ADR 0001 – Grundarchitektur

- Status: angenommen
- Datum: 2026-11-18

## Kontext

tuning-sim braucht eine deterministische Physiksimulation (1 kHz), eine Echtzeit-Audiosynthese
(48 kHz) und eine moderne, gut gestaltbare UI. Die UI soll zuerst im Browser laufen und später
mit Tauri v2 als Desktop-App verpackt werden. Der Kern soll testbar sein, ohne Audio, Netzwerk
oder Dateien zu brauchen.

## Entscheid

1. **Hexagonale Architektur (Ports & Adapters) in einem Monorepo.** `domain/` enthält reine Logik
   (Motor, ECU, Aufladung, Antrieb, Dyno) ohne I/O. `ports/` definiert Schnittstellen
   (`IAudioSink`, `IStatePublisher`, `ICommandSource`, `IEngineRepository`, `ITableRepository`,
   `IClock`), `adapters/` implementiert sie (miniaudio, WebSocket, JSON-Dateien). Abhängigkeiten
   zeigen nach innen.
2. **simcore ist ein eigenständiger Headless-Prozess** (C++20). Die UI spricht mit ihm über
   **WebSocket auf `127.0.0.1`** (JSON, versioniertes Protokoll v1). Die Schnittstelle ist
   dauerhaft. Tauri startet simcore später als Sidecar.
3. **Audio läuft im Kern.** simcore erzeugt und spielt den Sound selbst (miniaudio, eigener
   Audio-Thread). Audiodaten gehen nie über die WebSocket-Schnittstelle.
4. Kein Microservice-Ansatz: eine Anwendung, zwei Prozesse (simcore und UI).

## Konsequenzen

- (+) Domain-Logik ist ohne Infrastruktur testbar (Golden Files aus `lab/`).
- (+) Die UI ist austauschbar und im Browser debugbar. Ein Fake-Modus erlaubt UI-Entwicklung ohne simcore.
- (+) Audio hat kurze Wege zu den Simulationsparametern und keine Netzwerklatenz.
- (−) Zwei Prozesse brauchen ein Protokoll, Versionierung und ein sauberes Starten und Beenden.
- (−) Strikte Thread-Regeln sind nötig (Audio-Callback ohne Allokation, Locks oder I/O).

## Alternativen

- **Alles in TypeScript/WebAudio:** einfacher Einstieg, aber schwächere Echtzeit-Garantien und
  kein deterministischer, nativ testbarer Kern.
- **C++ als Bibliothek per FFI in Tauri/Rust:** keine IPC, aber enge Kopplung, schwierige
  Builds, Rust-FFI als zusätzliche Disziplin.
- **Audio in der UI (Samples per WebSocket):** Latenz, Jitter und Bandbreite, deshalb verworfen.
