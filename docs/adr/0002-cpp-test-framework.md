# ADR 0002 – C++-Testframework: doctest

- Status: angenommen
- Datum: 2026-11-18

## Kontext

simcore braucht Unit-Tests (Tabelleninterpolation, Physik-Plausibilität, Golden-File-Vergleiche
mit Toleranz). plan.md §3 lässt doctest oder Catch2 v3 offen. Abhängigkeiten kommen per CMake
`FetchContent`. Erlaubt sind Lizenzen MIT/BSD/Zlib (CLAUDE.md, Arbeitsregel 5).

## Optionen

| | doctest 2.5.3 | Catch2 3.16.1 |
|---|---|---|
| Lizenz | MIT | BSL-1.0 (permissiv, aber nicht auf der Liste) |
| Kompilierzeit | sehr schnell (ein Header) | deutlich langsamer (Bibliothek) |
| Funktionen | TEST_CASE, SUBCASE, CHECK, `doctest::Approx` | zusätzlich Matcher, Generatoren, Benchmarks |
| Pflege | ruhiger | sehr aktiv |

## Entscheid

**doctest 2.5.3**, eingebunden per `FetchContent` als Release-Tarball mit SHA256-Prüfsumme
(`core/tests/CMakeLists.txt`). Jeder `TEST_CASE` wird mit `doctest_discover_tests` als eigener
CTest-Test registriert.

## Konsequenzen

- (+) Schnelle Builds, MIT-Lizenz, Syntax fast identisch zu Catch2 (ein späterer Wechsel wäre billig).
- (+) Toleranzvergleiche für Golden Files mit `doctest::Approx(x).epsilon(...)`.
- (−) Keine eingebauten Benchmarks. Falls nötig, später separat (z. B. google/benchmark, nach Rückfrage).
