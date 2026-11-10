# Build, local play and GitHub Pages

## Versions

- C++ standard: C++20, extensions disabled.
- Emscripten: **4.0.23**, verified with `emcc --version` after loading `emsdk_env`. CI installs and activates this exact SDK through official emsdk; it never uses `latest`.
- CMake: **4.4.5** verified locally; project minimum **3.24**. CI uses the version supplied by `ubuntu-24.04` and prints it in the log.
- raylib: **5.5**, fetched by CMake from the official repository.
- Native verification: GCC16.2.0, Ninja1.13.2 on Windows. Python3 is used for static packaging and local HTTP serving.

## Windows

From the existing repository directory:

```powershell
.\scripts\check_environment.ps1
.\scripts\build_native.ps1
.\scripts\test_native.ps1
.\scripts\run_native.ps1
.\scripts\build_web.ps1
.\scripts\serve_web.ps1
```

Open `http://localhost:8000`. Use HTTP, not a file URL. Click the canvas to focus. To check native rendering automatically, run `scripts/run_native.ps1 -Smoke` (120 frames, screenshot, exit).

Tool discovery uses PATH and these optional environment variables:

| Variable | Value |
|---|---|
| LAST_CONNECTION_CMAKE | Full path to cmake executable |
| LAST_CONNECTION_NINJA | Full path to ninja executable |
| LAST_CONNECTION_GCC_ROOT | Directory containing gcc and g++ |
| LAST_CONNECTION_EMSDK | emsdk root, otherwise EMSDK |
| LAST_CONNECTION_PYTHON | Working Python3 executable |

An ignored `.local/toolchain.local.ps1` may override `$CMake`, `$Ninja`, `$GccRoot` and `$EmsdkRoot`. Machine paths stay in this file, not committed scripts. The existing workstation has such an override; no permanent PATH or installation location was changed.

Native output is `build/native-Debug/last_connection.exe`; Web output is `build/web/index.html`, `index.js`, `index.wasm`. No external asset/data package is currently required.

## Static distribution

```sh
python3 scripts/package_web.py --build build/web --output build/pages
python3 -m http.server 8001 --bind 127.0.0.1 --directory build/pages
```

On Windows use the Python3 interpreter returned by `Get-PreviewPython` from `scripts/toolchain.ps1`, if it is not on PATH. `build/pages` contains only runtime HTML/JS/WASM plus `.nojekyll`; the packager also supports optional `index.data`, `index.worker.js` and `assets/` if later required. Neither build trees nor distribution binaries are committed.

The HTML refers to `index.js` relatively, and the JS resolves `index.wasm` relative to its own URL. The site therefore works under a GitHub project subpath rather than assuming a domain-root install. Packaging checks reject root-relative HTML resources and a root-relative WASM loader. Local serving requires no compiler at runtime.

## GitHub Actions

`.github/workflows/deploy-pages.yml` runs on source/config pushes to main, or manually:

1. Official `actions/checkout@v6`.
2. Official emsdk tag4.0.23, install/activate SDK4.0.23.
3. `emcmake cmake -S . -B build/web -G Ninja -DCMAKE_BUILD_TYPE=Release -DPLATFORM=Web`.
4. Build `last_connection`, package `build/pages`.
5. Official `actions/upload-pages-artifact@v4`.
6. Official `actions/configure-pages@v5` and `actions/deploy-pages@v4` in the `github-pages` environment, with scoped `pages: write` and `id-token: write`.

The deploy job requires repository variable **PAGES_ENABLED=true**, now enabled. The initial Private-repository plan limitation was resolved after the owner explicitly authorized Public visibility. Pages Source is GitHub Actions. No gh-pages branch or generated files on main are used.

CI runs a black-box Chromium smoke test against the packaged local site **before deployment** and then runs the same test against the public Pages URL from a fresh job/context. It verifies HTTP/WASM loading, visible Bike interception via keyboard, matching vehicle/rider motion, CENTER return, finish result, and best-record persistence after reload. It saves unmodified screenshots and a JSON report as Actions artifacts. It does not inject gameplay state, override time, or access a user's browser profile. Dependencies are pinned in tests/browser-requirements.txt. This automated browser test is distinct from the owner's manual playtest.

Native gameplay suites may also run as actual WebAssembly modules under the SDK's Node with `scripts/test_web.ps1`. Large test functions retain several RNG/traffic fixtures and need a test-only 1MB stack (default64KB overflow diagnosed with STACK_OVERFLOW_CHECK); the playable application link and gameplay parameters are unchanged.

Project URL returned by Pages configuration: `https://kikisukii.github.io/Last-Connection/`. Deployment/verification status is recorded in FIRST_PLAYABLE.md.

Official references: [custom Pages workflows](https://docs.github.com/en/pages/getting-started-with-github-pages/using-custom-workflows-with-github-pages), [Pages REST API](https://docs.github.com/en/rest/pages/pages), [versioned emsdk installation](https://emscripten.org/docs/tools_reference/emsdk.html).
