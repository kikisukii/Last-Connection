# First Playable preparation — 2026-11-10

The owner confirmed actual playtesting and accepted the current V0.3 gameplay as a First Playable / Playtest Build. This does not mean final balance. The deployment task changes build tooling and documentation only; gameplay source hashes are unchanged.

## Completed checks

- Native Debug build PASS.
- All 12 CTest suites PASS, including adjacent progression, miss, decay/recovery, one-use IDs, service exhaustion, independent lanes, same-lane safety, finish/rank and best-time rules. The existing deterministic suites cover the thirteen requested gameplay smoke scenarios.
- Native 120-frame graphical startup smoke PASS, normal exit.
- Web Release build PASS with Emscripten4.0.23.
- Runtime package contains index.html, index.js, index.wasm and .nojekyll; no data/assets package is currently required. Loader paths are relative.
- Local Edge page loaded, rendered the canvas and accepted the start key. Further browser automation stopped because the tool could not confidently identify the current Windows browser URL. This run does **not** claim completed browser boarding, rank or storage verification.

## WebAssembly parity

The movement-feel, traffic/service and time-attack contracts also pass compiled to WebAssembly under SDK Node:240,367506 and56 assertions respectively. They exercise the real C++ gameplay code. The traffic/service test retains many fixtures on its main stack; a diagnosed default64KB stack overflow was resolved with a test-only1MB stack. This does not alter the playable game's link configuration.

## Repository and release state

The owner explicitly authorized changing this repository to Public and continuing Pages deployment. The API confirmed public visibility, enabled Pages with Source=workflow and enabled PAGES_ENABLED. Existing initial README commit e12e53a is retained as parent history; no reset/force push is used.

Machine tool paths live in ignored .local/toolchain.local.ps1. Documentation paths were redacted and original copies retained privately. Source/hash and credential-pattern audits passed; SDKs/build files/caches/local helpers remain excluded.

## Browser verification route

Desktop browser UI control was stopped by the tool's URL-identification check, even after the owner restored the window. No further desktop interaction is attempted. The local page/canvas/start were observed, but complete desktop boarding/finish/storage is not claimed. To complete technical browser validation independently, the Actions workflow uses a fresh temporary Chromium context on the runner: test the packaged site before deployment, then the actual public URL in a separate clean job, saving screenshots and result.json. The test does not inject gameplay state or accelerate simulation. CI results and the verified live link will be recorded after completion. Manual Edge/Chrome playtesting remains the owner's acceptance check.
