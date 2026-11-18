# Third-Party Notices

This project uses the third-party components listed below. Only permissive licences
(MIT, BSD, Zlib, Public Domain / MIT-0) are accepted. GPL projects (e.g. rusEFI, Speeduino)
are used as a source of concepts only; no code is copied.

Development-only tools that are not shipped with the application (compilers, linters,
formatters, test runners, build tools such as Vite, ESLint, Prettier, pytest, ruff) are not
listed here; their versions are recorded in `docs/setup.md`.

## Shipped with the application

### React and React DOM 19.3.1
- Source: https://github.com/facebook/react
- Licence: MIT
- Used in: `ui/`, UI framework (bundled into `ui/dist`)
- Integration: npm

Copyright (c) Meta Platforms, Inc. and affiliates.

### Tailwind CSS 4.3.5
- Source: https://github.com/tailwindlabs/tailwindcss
- Licence: MIT
- Used in: `ui/`, generated CSS (incl. Preflight base styles) bundled into `ui/dist`
- Integration: npm

Copyright (c) Tailwind Labs, Inc.

## Used in tests only

### doctest 2.5.3
- Source: https://github.com/doctest/doctest
- Licence: MIT
- Used in: `core/tests/`, unit test framework (not linked into `simcore`)
- Integration: CMake FetchContent

Copyright (c) 2016-2023 Viktor Kirilov

## Used in the Python lab (not shipped)

numpy (BSD-3-Clause), matplotlib (matplotlib License, PSF-based). See `docs/setup.md`.

## Template

```
### <Name> <version>
- Source: <URL>
- Licence: <SPDX identifier, e.g. MIT>
- Used in: <core/ | ui/ | lab/>, <purpose>
- Integration: <FetchContent | npm | pip | vendored>

<Full licence text or copyright line, as required by the licence>
```
