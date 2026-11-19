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

### uPlot 1.6.32
- Source: https://github.com/leeoniya/uPlot
- Licence: MIT
- Used in: `ui/`, dyno charts (bundled into `ui/dist`)
- Integration: npm

Copyright (c) 2022 Leon Sorokin

### miniaudio 0.11.26
- Source: https://github.com/mackron/miniaudio
- Licence: Public Domain (Unlicense) or MIT No Attribution (MIT-0), at our choice
- Used in: `core/spikes/sound_spike`, audio output (later simcore)
- Integration: CMake FetchContent (release tarball + SHA256)

Copyright 2025 David Reid (MIT-0 alternative; no attribution required)

### nlohmann/json 3.12.0
- Source: https://github.com/nlohmann/json
- Licence: MIT
- Used in: `core/` (simcore), engine/table files and the WebSocket protocol
- Integration: CMake FetchContent (release tarball + SHA256)

Copyright (c) 2013-2025 Niels Lohmann

### IXWebSocket 12.1.0
- Source: https://github.com/machinezone/IXWebSocket
- Licence: BSD-3-Clause
- Used in: `core/` (simcore), WebSocket server (ADR 0004)
- Integration: CMake FetchContent (release tarball + SHA256), without TLS and zlib

```
Copyright (c) 2018 Machine Zone, Inc. All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are
met:

1. Redistributions of source code must retain the above copyright
   notice, this list of conditions and the following disclaimer.

2. Redistributions in binary form must reproduce the above copyright
   notice, this list of conditions and the following disclaimer in the
   documentation and/or other materials provided with the
   distribution.

3. Neither the name of the copyright holder nor the names of its
   contributors may be used to endorse or promote products derived
   from this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
"AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
```

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
