# setup.md – Toolchain und Versionen

Ergebnis der Systemprüfung (Iteration 0, 2026-11-18).

## System

| | |
|---|---|
| OS | Fedora Linux 43 (KDE Plasma Desktop Edition), x86_64 |
| Kernel | 7.3.4-100.fc43 |
| IDE | CLion 2026.3.0.1 (C++), IntelliJ Ultimate (TS, Python), beide über JetBrains Toolbox 3.9.1 (`~/.local/opt/jetbrains-toolbox`) |

## CLion-Einrichtung

- Repo-Root öffnen (nicht `core/`).
- Toolchain «Default»: **System CMake** (`/usr/bin/cmake`, nicht Bundled), damit Terminal und IDE dieselbe CMake-Version im selben Build-Ordner nutzen. Ninja, GCC und Bundled GDB sind automatisch erkannt.
- CMake-Profile: nur **«debug - debug preset»** aktiv (Build-Ordner `build/debug`). Das von CLion angelegte Profil «Debug» (`cmake-build-debug`) ist deaktiviert.

## Tools

| Tool | Version | Anforderung | Installation (Fedora) |
|---|---|---|---|
| git | 2.56.1 | – | `sudo dnf install git` |
| GitHub CLI (gh) | 2.87.3 | – | `sudo dnf install gh`, danach `gh auth login` |
| CMake | 3.31.11 | ≥ 3.25 | `sudo dnf install cmake` |
| Ninja | 1.13.1 | – | `sudo dnf install ninja-build` |
| GCC (g++) | 15.3.1 | C++20 | `sudo dnf install gcc-c++` |
| clang-format | 21.1.8 | – | `sudo dnf install clang-tools-extra` |
| Clang (clang++) | nicht installiert | optional | `sudo dnf install clang` |
| Python | 3.14.10 | ≥ 3.12 | vorinstalliert (`python3`) |
| Node.js / npm | 22.23.5 / 10.9.9 | Node ^20.19 oder ≥ 22.12 (Vite 8) | `sudo dnf install nodejs` |

`gh` ist mit dem GitHub-Konto angemeldet, Git-Protokoll SSH.
Commit-Mail in diesem Repo (lokale git config): GitHub-Noreply-Adresse, weil das Repo öffentlich ist.

## Bibliotheken

Ermittelt am 2026-11-18 aus den offiziellen Quellen (GitHub Releases, PyPI, npm-Registry), nicht aus dem Gedächtnis.
Exakte UI-Versionen stehen in `ui/package-lock.json`.

### C++ (`core/`, per FetchContent in `core/cmake/Dependencies.cmake`)

| Bibliothek | Version | Lizenz | Quelle | Hinweis |
|---|---|---|---|---|
| doctest | 2.5.3 | MIT | github.com/doctest/doctest | Tarball + SHA256, nur Tests (ADR 0002) |
| miniaudio | 0.11.26 | Public Domain / MIT-0 | github.com/mackron/miniaudio | Tarball + SHA256, eigenes CMake-Target, ohne Decoder/Engine (ADR 0003) |

### Python (`lab/`, in `lab/.venv`)

| Paket | Version | Lizenz | Hinweis |
|---|---|---|---|
| numpy | 2.5.5 | BSD-3-Clause | `>=2.5,<3` |
| matplotlib | 3.11.2 | PSF-basiert (matplotlib License) | `>=3.11,<4` |
| pytest | 9.2.0 | MIT | dev |
| ruff | 0.17.5 | MIT | dev, **exakt gepinnt** (Formatierung ändert sich zwischen Minor-Versionen) |
| hatchling | 1.33.0 | MIT | Build-Backend |

### UI (`ui/`, npm)

| Paket | Version | Lizenz | Hinweis |
|---|---|---|---|
| react, react-dom | 19.3.1 | MIT | Laufzeit |
| vite | 8.3.8 | MIT | Gerüst mit create-vite 9.3.4 (Template `react-ts`, `--eslint`) |
| @vitejs/plugin-react | 6.3.0 | MIT | |
| typescript | 6.0.3 (`~6.0.3`) | Apache-2.0 | **Nicht** 7.x: typescript-eslint 8.73 unterstützt nur TS < 6.1 |
| tailwindcss, @tailwindcss/vite | 4.3.5 | MIT | Tailwind v4: Konfiguration per `@theme` in CSS statt `tailwind.config.js` |
| eslint | 10.15.0 | MIT | statt Oxlint (create-vite-Standard), wie in plan.md festgelegt |
| typescript-eslint | 8.73.0 | MIT | `strictTypeChecked` + `stylisticTypeChecked` |
| prettier | 3.10.1 | MIT | |
| eslint-config-prettier | 10.1.8 | MIT | |

### CI (GitHub Actions, `.github/workflows/ci.yml`)

| Action / Tool | Version | Hinweis |
|---|---|---|
| actions/checkout | v7 (7.0.1) | |
| actions/setup-python | v7 (7.1.0) | Matrix Python 3.12 + 3.14 |
| actions/setup-node | v7 (7.1.0) | Node 22 |
| Runner | ubuntu-24.04 | CMake 3.31, Ninja 1.13, GCC 14 (`CXX=g++-14`) |
| clang-format (PyPI) | 21.1.8 | gleiche Version wie lokal |
