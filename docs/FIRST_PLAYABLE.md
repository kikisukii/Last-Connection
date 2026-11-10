# First Playable delivery — 2026-11-10

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

Desktop browser UI control was stopped by the tool's URL-identification check, even after the owner restored the window. No further desktop interaction was attempted. The local page/canvas/start were observed, but complete desktop boarding/finish/storage is not claimed. Technical browser validation instead passed independently in fresh temporary Chromium contexts on the Actions runner: the packaged site before deployment, then the actual public URL in a separate clean job. The test does not inject gameplay state or accelerate simulation. Manual Edge/Chrome playtesting remains separate from these automated checks.

## Verified public deployment

- [Play in browser](https://kikisukii.github.io/Last-Connection/).
- [Actions run 42805243312](https://github.com/kikisukii/Last-Connection/actions/runs/42805243312): Build Web, Deploy Pages and Verify public browser demo all PASS.
- Tested build source: `305e9aa84685bb030aae775cd27596c004bdc0cb` (`feat: prepare first playable web demo`). Follow-up changes only add this delivery record, evidence and README link; they do not alter build inputs or gameplay.
- Packaged local site: fresh Chromium context, runtime/WASM loaded, both arrow inputs, Bike catch/follow and return to CENTER PASS. Finished in 98.7278 game seconds with rank F; best time survived reload; no page errors.
- Public Pages site: separate fresh Chromium context, the same checks PASS. Finished in 98.7611 game seconds with rank F; best time survived reload; no page errors. WASM returned HTTP 200 with the correct magic bytes and MIME type.
- Screenshot inspection confirms Bike speed 20 and SUCCESS, the public finish panel showing `01:38.76` / `RANK F`, and the reloaded start panel retaining `BEST 01:38.76`. This deliberately brief Bike ride followed by running tests late-arrival settlement, not optimal play or final balance.
- Evidence: [local result](evidence/first-playable-local-browser.json), [public result](evidence/first-playable-public-browser.json), [boarding](evidence/first-playable-public-boarding.png), [finish](evidence/first-playable-public-finish.png), [reloaded best](evidence/first-playable-public-reloaded.png). Full screenshot sets are also attached to the Actions run.
- Milestone tag: `v0.1-playtest`, annotated `First playable gameplay prototype for external playtesting.`

No gameplay changes were made for deployment. Native tests cover the full progression and other ranks; browser automation above verifies one complete late-arrival path. Further external playtest feedback and balance tuning remain future work.
