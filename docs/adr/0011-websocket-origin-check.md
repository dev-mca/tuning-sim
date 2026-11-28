# ADR 0011 – Origin-Prüfung im WebSocket-Server

- Status: vorgeschlagen
- Datum: 2026-11-28

## Kontext

simcore hört nur auf 127.0.0.1, aber jede Webseite im Browser des Nutzers kann
`ws://127.0.0.1:47800` öffnen und simcore steuern (ADR 0006, Backlog). Browser schicken beim
Handshake den `Origin`-Header der Seite. Eine Seite kann ihn nicht fälschen. Clients ohne Browser
(Node-e2e-Test, CLI-Werkzeuge) schicken keinen Origin. IXWebSocket-Clients schicken ihre eigene URL
(`ws://127.0.0.1:<port>`) als Origin.

IXWebSocket 12.1.0 bietet keinen Haken vor der `101`-Antwort. Die Header stehen erst in der
`Open`-Nachricht (`openInfo.headers`).

## Optionen

| Option | Vorteil | Nachteil |
|---|---|---|
| **Loopback-Host mit beliebigem Port** (http, ws, wss) + feste Tauri-Origins | Vite darf auf 5174, 5175 … ausweichen, `vite preview` (4173) geht | jede andere lokale Webseite auf localhost darf auch verbinden |
| Feste Liste (nur `:5173`) | am engsten | bricht den Dev-Ablauf, sobald Vite den Port wechselt |
| CLI-Flag `--allow-origin` | flexibel | mehr Oberfläche, Änderung in `app/cli` |

## Entscheid

`adapters/origin_policy` (reine Funktion `isAllowed`) erlaubt:

- keinen Origin-Header (Clients ohne Browser),
- `http://`, `ws://`, `wss://` mit `localhost`, `127.0.0.1` oder `[::1]` und beliebigem Port,
- `tauri://localhost`, `http://tauri.localhost`, `https://tauri.localhost` (Tauri v2).

Schema und Host werden ohne Rücksicht auf Gross-/Kleinschreibung verglichen (RFC 6454). Alles
andere wird abgelehnt: fremde Hosts, `null` (Sandbox-iframes, lokale Dateien) und Werte mit Pfad
oder Schrägstrich am Ende. Der Server schliesst eine abgelehnte Verbindung direkt nach dem
Handshake mit dem Code 1008 (Policy Violation), registriert sie nicht und ignoriert ihre Befehle.

## Konsequenzen

- (+) Fremde Webseiten (auch per DNS-Rebinding, ihr Origin bleibt fremd) steuern simcore nicht mehr.
- (+) Dev-Server, Tauri, Node-e2e-Test und C++-Tests verbinden sich weiter ohne Konfiguration.
- (−) Abgelehnte Clients sehen kurz ein `open` und dann ein `close` statt einer HTTP-403-Antwort.
- (−) Andere lokale Webserver auf localhost gelten als vertrauenswürdig.
