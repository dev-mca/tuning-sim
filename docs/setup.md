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

`gh` ist mit dem GitHub-Konto angemeldet, Git-Protokoll SSH.
Commit-Mail in diesem Repo (lokale git config): GitHub-Noreply-Adresse, weil das Repo öffentlich ist.

## Bibliotheken

_Noch keine. Ab Iteration 1 hier mit ermittelter aktueller stabiler Version eintragen (nicht aus dem Gedächtnis)._

| Bibliothek | Version | Lizenz | Ermittelt am | Quelle |
|---|---|---|---|---|
