# Three-language UI

Entry always shows **SELECT LANGUAGE → 中文 / English / 日本語**. An explicit
selection opens the translated tutorial. Arrows cannot bypass selection.
Language lives only in the presentation `Flow`; optional help and direct
replay keep it. Reloading creates a new Flow and asks again. Best-time storage
remains independent and unchanged. There is no browser-language detection,
language storage, account, network translation service or i18n dependency.

`include/localization_data.inc` is the UTF-8 source of truth for tutorial,
lane/vehicle names, normal HUD, prompts and result/replay labels.
`getText(Language, key)` is shared by Native drawing and the HTML menu bridge.
Debug diagnostics remain English. HTML loading status is removed when ready.

The Native menu uses three cards; UTF-8 wrapping keeps Latin words together and
breaks CJK at codepoint boundaries. Web has semantic buttons, responsive cards,
and measured fitting for short viewports. Bundled SC/JP subsets supply both the
HTML and raylib UI; [font provenance, license and sizes](../assets/fonts/README.md).
All source text is UTF-8; MSVC receives `/utf-8`. GCC/Emscripten use UTF-8 input.

## Verification scope

- Native Windows GCC build and all 14 CTest suites pass. The UI suite checks
  three language flows through real TimeAttack simulation, finish/help/replay,
  session retention and fresh-load selection. This is automated model coverage,
  not a claim of three manual Native playtests.
- Native graphics smoke captures language selection and all three tutorial
  screens. Both raylib fonts load 380 glyphs; screenshots were visually checked.
  MSVC configuration is included, but no MSVC build was available/run.
- Local fresh Chromium contexts each pass an unaccelerated full run: explicit
  selection, tutorial, mouse/arrow start without transfer, actual Bike catch and
  follow/release, finish time/rank, optional help/back, direct replay, reload and
  saved best. No game-state injection or balance overrides.
- Eight viewport sizes per language: 1400×820, 1024×600, 800×600, 640×480,
  640×360, 390×844, 360×640, 320×568. Text and START remain in the viewport
  without page scrolling. Screenshots include Chinese and Japanese small views,
  HUD and result screens. Debug is opt-in and absent from menus.
- The same three-language browser checks gate the Pages build and run again
  against the deployed public URL. Deployment success is reported only after
  Actions and public verification finish.

Automated local evidence is retained under `docs/evidence/localization/`.
Public browser evidence is attached to the deployment Actions run.
**Pending playtest:** readability and first-player comprehension.

No gameplay source/configuration, speeds, transfer rules, thresholds, Catch
Zones, services, traffic/TTC, rank/deadline or finish distance changed.
