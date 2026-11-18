# ADR 0004 – WebSocket-Bibliothek: IXWebSocket

- Status: angenommen
- Datum: 2026-11-18

## Kontext

simcore stellt seine dauerhafte Schnittstelle als WebSocket-Server auf 127.0.0.1 bereit
(ADR 0001, plan.md §6). plan.md §3 nennt als Kandidaten IXWebSocket, uWebSockets und
Boost.Beast. Anforderungen: Server-Unterstützung, Einbindung per CMake FetchContent, Lizenz
MIT/BSD/Zlib (CLAUDE.md, Arbeitsregel 5), wenig Abhängigkeiten, gut verständlich.

## Optionen

| Option | Vorteil | Nachteil |
|---|---|---|
| **IXWebSocket 12.1.0** (BSD-3-Clause) | eigenes CMake-Projekt, Client und Server, Callbacks pro Verbindung, ohne TLS/zlib baubar | eigene Threads pro Verbindung (für einen lokalen Client unkritisch) |
| uWebSockets 20.81 (Apache-2.0) | sehr schnell, Event-Loop | Lizenz nicht auf unserer Liste, braucht uSockets, Makefile statt CMake |
| Boost.Beast (BSL-1.0) | sehr verbreitet, flexibel | zieht Boost (Asio) mit, viel Boilerplate, steile Lernkurve |

## Entscheid

**IXWebSocket 12.1.0** per FetchContent (Tarball + SHA256) mit `USE_TLS=OFF` und `USE_ZLIB=OFF`.
Nur `adapters/websocket_server.cpp` (und ein Integrationstest) kennt IXWebSocket. Das
JSON-Protokoll liegt in einem eigenen Codec (`adapters/protocol.cpp`) mit nlohmann/json 3.12.0
(MIT, in plan.md §3 bereits festgelegt).

## Konsequenzen

- (+) Austauschbar: Der Rest von simcore sieht nur die Ports und den `MessageBus`.
- (+) Der Integrationstest nutzt den IXWebSocket-Client, also kein zweites Werkzeug.
- (+) Keine System-Abhängigkeiten (OpenSSL, zlib) und CI ohne Zusatzpakete.
- (−) Keine Kompression und kein TLS. Für 127.0.0.1 nicht nötig. Für einen Fernzugriff müsste man
  das neu bewerten.
- (−) Netzwerk-Callbacks laufen auf mehreren Threads. Deshalb serialisiert ein Mutex die
  Produzenten vor der lock-freien Befehlsqueue (siehe `adapters/message_bus.hpp`).
