# Verified development record

This file records engineering observations. It does not invent user playtests.

## 2026-11-09 — Phase 0

- **Changed:** inspected the existing empty project directory, PATH, common installation directories, D: files and installed-program entries; initialized a local Git repository on `main`.
- **Why:** avoid duplicate installations and keep the source at the exact user-specified path.
- **Observed:** Git 2.55.0 and PowerShell 7.6.9 work. The Python launcher has no registered interpreter; Codex's bundled Python 3.12.14 works. No CMake, native compiler, Ninja or Emscripten found in the inspected locations. Old Python 3.10 MSI entries alone do not prove a usable installation.
- **Resolved:** installed CMake 4.4.5, Ninja 1.13.2 and w64devkit 2.11.0 / GCC 16.2.0 in D: portable directories. CMake archive SHA-256 matched the publisher checksum file. Version commands passed. No machine/user PATH was changed.
- **Resolved subsequently:** Emscripten 4.0.23 installed/activated in `<local tool path>`, including SDK-managed Python 3.13.3 and Node 24.19.0. The existing Codex Python was used to bootstrap the SDK; no separate standalone Python installation was added.

## 2026-11-09 — Phase 1 complete

- **Changed:** minimal C++20/raylib render loop, CMake configuration, native/web build scripts and localhost preview script; preserved the original brief.
- **Why:** verify the full build chain before adding gameplay.
- **Observed:** native build passed; a 120-frame native run initialized OpenGL 3.3 on the NVIDIA RTX 4070, rendered the scene, wrote `build/native-Debug/smoke.png` and exited successfully. WebAssembly build passed; Edge at `http://localhost:8000` visibly rendered the same rotating square and timer.
- **Resolved:** the complete native/browser render pipeline works with the original source path, including its spaces. raylib tag 5.5 resolved to `c1ab645ca298a2801097931d1079b10ff7eb9df8`.
- **Remaining:** raylib's old CMake minimum emits a deprecation warning; its bundled stb_vorbis emits a Web compiler tautological-comparison warning. Neither blocked compilation. No gameplay or user playtest claim follows from this smoke test.

## 2026-11-09 — Phase 2 movement sandbox

- **Changed:** separated pure C++ movement from raylib rendering/input; added centralized configuration, fixed-step update, analytical timed lateral movement, a debug overlay and deterministic tests. Kept the Phase 1 smoke program as a separate target.
- **Why:** make adjacency, timing and continued forward motion inspectable before introducing vehicles.
- **Observed:** native and Web builds pass. CTest passes all 34 checks in `movement_contract`. The native 120-frame render smoke exits successfully and writes `sandbox.png`.
- **Found and resolved:** an exact-duration regression test initially failed: 30 additions of 1/120 s fell slightly below 0.25 s. A machine-epsilon-scale completion tolerance prevents a spurious extra tick. The failing case is retained as a test.
- **Found and resolved:** short browser taps were not reliably observed with held-state comparison (`IsKeyPressed`). Input now consumes raylib's press-event queue (`GetKeyPressed`), which records press edges and excludes held-key repeats. Browser interaction subsequently showed CENTER → RIGHT → CENTER → LEFT and blocked both outer-boundary directions, with speed remaining 12 and forward position increasing. Screenshot: `docs/evidence/phase2-web.png`.
- **Not claimed:** human playtesting, final transfer feel, catch timing, recovery balance, finish/rank or deployment. A desktop-window focus-inspection attempt timed out awaiting UI-control permission; browser verification continued through the already available browser interface.

### User feedback after Phase 2

- User reported: “左右可以移动了，没有问题” (left/right movement works, no problem reported).
- This confirms the user's basic directional-input playtest. It does not establish final tuning, vehicle/catch behavior or a broader usability test.

## 2026-11-09 — Phase 3 vehicle streams

- **Changed:** added an authored eight-vehicle sequence using all four requested types; speeds and body lengths come from the brief and remain configurable. Initial positions and lanes are also centralized in `GameplayConfig`.
- **Why:** make relative movement and repeatable vehicle arrivals visible before catch logic.
- **Behavior:** vehicle centers follow `initialPosition + speed * simulationTime` on the same clock as the player. Rendering uses simple colored rectangles with lengths in world units. Culling does not remove or respawn vehicles. The finite sequence can be replayed by reloading/relaunching.
- **Temporary setup:** at a constant player speed of 12, vehicle centers pass at 2/5/8/11/14/18/20/23 simulation seconds. These are inspection fixtures, not a tuned or approved catch sequence. The second bus can support a later recovery scenario, but no recovery mechanics are implemented now.
- **Observed:** Native Debug and Web Release builds passed. CTest passed both suites: 34 movement checks and 69 vehicle checks. Native render smoke showed a bike alongside the player at approximately 2 seconds and exited normally. Edge displayed a right-lane bus at simulation time 5.04 seconds with speed 32 and relative speed +20, while the player could move to LEFT and retained speed 12. The finished-sequence message was observed; reload restarted the sequence. Screenshots are in `docs/evidence/phase3-native.png` and `phase3-web.png` (the latter is between visible vehicles).
- **Resolved:** traffic uses repeatable trajectories, stable IDs, outer lanes and configurable geometry without adding contact-based mounting. No new software was installed for this phase.
- **Uncertain:** catch windows, target selection, attachment semantics and actual user experience with traffic remain for subsequent work.

## 2026-11-09 — Phase 3 feedback: visible vehicles must not overlap

- **User observation:** asked whether arrivals were random; reported a larger vehicle passing through a bike. Explicit constraint: “画面内不能相撞”; off-screen traffic does not need collision simulation.
- **Diagnosis:** the initial sequence was deterministic but each body followed an independent trajectory. A concrete overlap was reproduced at 20.5 s between right-lane Bike #6 and Taxi #7, near the front of the viewport.
- **Changed:** added a deterministic entry planner using body lengths, relative speeds and the shared viewport bounds. It moves a conflicting starting position further back so the vehicle enters later without changing speed. Added configurable `vehicleMinimumGap = 1.0` and nearest-vehicle entry-delay debug information. No other gameplay phase was added.
- **Timing impact:** only Taxi #7 changes in the default sequence: 0.172931 s later. Its speed stays 45; other vehicle entries remain unchanged. Off-screen passing remains possible.
- **Tests:** 34 movement checks and 242 vehicle checks pass, including clearance over full shared-visibility intervals, crowded entries, altered camera bounds, stable replay and the reported overlap fixture. An intermediate test exposed seed validation occurring after type indexing; validation was moved before planning. Both CTest suites now have 15-second timeouts.
- **Build/runtime:** final native and Web builds are checked separately. The Web page loads and runs with the new gap/delay overlay. This round's native graphical check failed during GLFW initialization with “Failed to determine Monitor to center Window”; a bounded retry also failed. It did not reach the simulation or render loop. A temporary native snapshot helper was removed; no new gameplay inputs were retained. Earlier successful native screenshots remain historical evidence, not evidence of this revision's rendering.
- **Remaining limit:** clearance is proven for the current constant normal-speed camera. Before enabling riding or speed changes, update the planner for actual camera motion and add corresponding tests. User confirmation of this revision's visual fix is still pending.

## 2026-11-09 — User-authorized equal-probability random traffic

- **User instruction:** “车的出现逻辑写成随机的，暂时先定成几个概率一样”. This supersedes the original brief's fixed live sequence for vehicle generation.
- **Changed:** live traffic draws all four types at 25% each and the two transport lanes at 50% each. Initial timing: first request at 1 s, then random 2–4 s intervals after actual entry. All probabilities, timing and seed controls are centralized in `GameplayConfig`.
- **Why:** provide the requested random arrival experience while keeping debugging reproducible with a fixed seed.
- **Preserved:** visible same-lane clearance, configured vehicle speeds, three-lane movement and no catch logic. A delayed draw is kept rather than replaced by a new type. No per-four-car quota is imposed.
- **Implementation:** continuous bounded active history, deterministic mt19937 mapping, optional fixed seed, timestamp-based seed on ordinary restart. Retained the old finite fixture only for regression tests.
- **Test observations:** all three CTest suites pass. Historical suites retain 34 movement and 242 vehicle checks. Random tests cover replay/frame independence, probabilities, cleanup and full visible-interval separation across 64 dense seeds. Seed 20261109 produced 33,111 draws with counts 8,285/8,252/8,267/8,307 and 16,503 LEFT choices.
- **Runtime verification:** final native and Web builds passed. Edge showed a random right-lane Bike, successful player movement to RIGHT, normal speed 12 and the gap overlay. Refresh changed the displayed seed and reset the run; empty startup handled correctly. Screenshot: `docs/evidence/random-traffic-web.png`. Native graphical launch was not repeated after the preceding GLFW monitor failure.
- **Remaining:** user feedback on spawn pace and random traffic; catch mechanics and variable-camera admission still belong to later work.

## 2026-11-09 — Yellow Taxi appears suddenly near the bottom

- **User report:** short vehicles appeared to pop into the lower road; clarified that Bike seemed normal and the yellow Taxi was the noticeable case. No user recording or measured frame rate was supplied.
- **Diagnosis/evidence:** entry geometry places the front bumper on the bottom correctly. A reproducible long-frame path nevertheless makes its first rendered sample late. At 99 ms after scheduled entry, Taxi exposes about 42.47 pixels / 65.34% of its 65-pixel body; Bike exposes 10.30 pixels / 26.4%. This is a verified mechanism consistent with the report, not proof of the user's exact frame history.
- **Regression first:** introduced a shared `FrameClock` and `RoadView` plus an integration test of first-visible geometry. The previous 0.1 s budget failed with “short car first appears mostly inside the road after a long frame”.
- **Changed:** reduced the common frame-advance budget to 1/60 s, at most two default physics steps between draws. No car-specific slowing, spawn-position teleport, probability change or clearance-rule change. Excess stall time is discarded for all objects together. Moved road projection into a pure helper used by both renderer and tests.
- **Tradeoff announced:** low-FPS/stalled runs now advance simulation more slowly than wall time. This must be revisited before ranking finish times; it is not a final timing/balance decision.
- **Verification:** Native and Web builds passed, as did all four CTest suites. Entry checks cover four body lengths, off-frame-boundary entry, simulated 144/60/30/10 FPS and 500 ms stalls, preserved world trajectories/speeds and first visibility still connected to the road bottom, at most 12% of the current body lengths. Browser loads the final revision; no claim of captured first-frame Taxi video is made. Native graphical launch was not repeated after the earlier GLFW monitor failure.
- **Still pending:** user confirmation that the originally observed yellow-car appearance is resolved. The actual original frame history remains unknown.

## 2026-11-09 — Phase 4 continuous catches and riding

- **User confirmation:** “现在OK了，进行下一阶段操作” confirms the preceding short-car entry fix and authorizes the next phase. This does not establish final performance or balance.
- **Explicit design choice:** asked when speed should be inherited after a mid-window intersection. User selected “交会时开始跟随车辆速度，横移继续完成（推荐，可避免位置跳变）”. This resolves the earlier attachment timing question.
- **Changed:** pure continuous relative-position detector, shorter centered catch zones, configurable relative-speed threshold, earliest eligible target selection, event-time integration, Riding state, departure with retained momentum and distinct miss debug reasons. The renderer shows catch geometry plus target/state/window data. No Phase 5 penalties or decay were introduced.
- **Why:** detect an intersection anywhere in the transfer window, including between sampled frames, and follow the target without a forward-position snap. Merely standing in the transport lane does not catch passing traffic.
- **Initial tuning:** zone lengths 2 / 7 / 3.5 / 11, relative-speed limit 30. At initial run speed 12, Bike and Bus qualify by speed; Taxi and Express require prior momentum. These are provisional values for user review, centralized in `GameplayConfig`.
- **Traffic dependency:** the Phase 3 fixed-camera planner could not safely support riding. `LiveTraffic` now places entrants outside the actual camera bounds, uses actual player speed to choose the entering edge, and requires nonclosing same-lane speed order plus bumper clearance. Faster followers wait off-screen. Each lane keeps one pending request; opportunities recur every 2–4 simulation seconds rather than waiting for a globally blocked admission. Type draws retain equal weights. Actual visible proportions depend on clearance and camera motion.
- **Tests observed:** all six CTest suites passed after the final code changes. New catch suite has 34 checks including mid-window capture, equal-speed/edge cases, threshold rejection, no snap, exact-end floating-point behavior, timestep partitioning, departure and a 20 → 45 → 32 → 60 momentum chain. Live traffic tests run 32 seeded changing-camera stress scenarios, check pair clearance and constant trajectories, seed replay/type proportions, both-edge first visibility at simulated 144/60/30/10 FPS plus 500 ms stalls, and a generated bike retained through over 30 seconds of riding. Existing four regression suites still pass. Results: `docs/evidence/phase4-tests.txt`.
- **Build/runtime observed:** Native Debug and Web builds passed. Edge displayed the final phase label, colored inner catch rectangles, a lateral transfer in progress, then LEFT / RUNNING / MISS with forward progress continuing at 12. Saved `docs/evidence/phase4-web.png` shows a Bus catch zone and the last miss result; it is not a successful-catch screenshot. Continuous catch/riding evidence this round comes from deterministic tests, not a captured browser catch. Captured browser error entries were from an unrelated Zotero extension; no game error was observed. Native graphical launch was not repeated after the earlier GLFW monitor failure.
- **Remaining:** user playtest of timing/attachment feel and provisional threshold/zone sizes. Departure velocity remains constant until Phase 5. Co-moving visible cars can remain alongside and hold up their lane's pending traffic, so this phase can have sparse traffic while riding/coasting. The prior shared frame budget and simulation-time slowdown remain provisional until the time-attack phase. No repository publication/deployment occurred.

## 2026-11-09 — Phase 5 miss, decay and recovery

- **User feedback/authorization:** user reported “上车没问题了”, confirming boarding works in their playtest, then requested “继续下一个阶段”. This authorizes Phase 5, not final balance claims or Phase 6.
- **Changed:** add gradual unmounted speed relaxation, once-only miss penalty at transfer completion, positive floor for repeated misses, DECELERATING/RECOVERING debug states, and before/after penalty feedback. Normal departure to CENTER has no penalty. Riding and early following do not decay. No new controls, death or position resets were added.
- **Parameters:** brief values deceleration 10 and miss penalty 5 retained. Recovery acceleration 5 and minimum forward speed 1 are provisional defaults announced during implementation. All four are centralized in `GameplayConfig`; normal speed remains equilibrium 12, not a minimum. The penalty floor never increases a speed already below it.
- **Prediction dependency:** the prior constant-velocity prediction no longer matched a decelerating/recovering player. The new solver partitions the piecewise quadratic relative trajectory at zone roots, speed-limit roots and equilibrium. It finds the earliest eligible event across the full transfer window and checks relative speed at that event. Actual movement uses the same exact forward integrator. A catch still inherits vehicle speed immediately and finishes lateral motion without snapping.
- **Event timing:** updates split at both catch and transfer completion. A miss in a long update deducts speed at the completion instant, then recovers/decays for the rest of that update. A boundary catch wins over a miss. Invalid input and subsequent updates cannot repeat the penalty.
- **Verification:** final Native Debug and Web builds passed; seven CTest suites passed. New coverage includes 12 → 7 → gradual 12, high-speed miss preserving momentum, slower-Bus recovery, positive floor under repeated misses, timestep independence, equilibrium/tangent/middle-window catches, speed-limit crossings and offset continuity. Eight fixed-seed 100-second integrated traffic runs produced 680 catches / 440 misses with no retained same-lane overlap. Added live entry coverage at player speed 1 while retaining all previous normal-speed and both-edge checks. Test report: `docs/evidence/phase5-tests.txt`.
- **Browser observation:** Edge loaded the final Phase 5 build, accepted a LEFT transfer, displayed the recorded `Miss: 12.00 -> 7.00 (-5.00)`, then showed current speed 12 / RUNNING with continued forward progress. `docs/evidence/phase5-web.png` captures the recovered state and retained penalty record. The sampled screenshot is not a recording of every intermediate recovery speed; those are covered by deterministic tests. The browser warning/error log was empty during this check.
- **Limitations:** native graphical launch was not retried after the previously recorded GLFW monitor initialization failure; this revision's native evidence is compilation/headless tests. No user Phase 5 playtest or final recovery tuning is claimed yet. Shared frame time remains capped, with the documented low-FPS slow-motion tradeoff. Finish/deadline/rank/best time belong to Phase 6; no publication occurred.

## 2026-11-09 — Phase 6 Time Attack loop

- **Authorization:** user requested “好的进行下一阶段”. Two explicit design choices followed: scores use game/simulation time so motion and clock slow together; runs continue past deadline and report actual finish time with F at the airport.
- **Changed:** `TimeAttack` wraps existing gameplay/traffic with a fixed finish, exact crossing time, progress, deadline/late status, S–F rank and frozen results. Added start/retry UI and local best-time storage. No extra movement keys, airport art, network service or deployment was added.
- **Prototype values announced:** finish distance 1200; S/A/B/C upper cutoffs 30/40/50/65 simulation seconds; D through the 90-second deadline; F afterward. Thresholds are inclusive and unrounded; values remain editable in `GameplayConfig`. No balance approval is implied.
- **Timing:** locating the finish via the real gameplay integrator avoids a tick-sized finish error or incorrect interpolation through catch/recovery events. The same accepted simulation time drives movement, traffic, score and deadline. Menu/loading time is excluded. Deadline only marks the run late; it does not stop the player.
- **Best/retry:** fastest completed time, including late completions, is retained on retry and stored locally. Browser uses origin-local localStorage; native uses a file beside the executable. Gameplay configuration is included in the record key. Corrupt/stale records are ignored; storage failure falls back to a session record. Retry resets the whole race and frame accumulator without turning the menu press into a lane move. Random mode uses a new seed.
- **Automated verification:** Native Debug and Web builds passed with no warning/error matches in the captured build logs. All eight CTest suites passed. The new 56-check race suite covers six ranks/boundaries, fractional-step finish across render rates/stalls, exact-deadline D, post-finish freezing, late arrival, restart, a quadratic recovery finish, actual generated-Bike riding finish, and record validation. Full CTest log copied to `docs/evidence/phase6-tests.txt`.
- **Browser verification:** Edge start screen waited for input; a RIGHT press started at CENTER rather than transferring. A no-input run finished at `01:40.00`, distance 1200, with `F / MISSED FLIGHT`, demonstrating continuation after the 90-second deadline. It saved `BEST TIME 01:40.00`. Retry reset time/position/lane/speed, changed the random seed and kept best time. Reload returned to the start screen with the best record still present. Browser warnings/errors were empty during these checks. Evidence: `docs/evidence/phase6-result-web.png` and `phase6-reloaded-web.png`. The browser currently retains this engineering-test record; it is not a user-achieved score.
- **Limits/next:** native graphics was not retried after the earlier GLFW monitor failure; native persistence was compiled and record parsing tested, but its UI/disk round trip was not exercised in a graphical run. Ranks other than F and successful riding finish are supported by automated tests, not claimed as manually captured browser completions. Phase 7 will broaden browser verification; Phase 8 deployment and Phase 9 polish remain separate. User playtest of distance/rank/deadline balance is pending.

## 2026-11-10 — Targeted Traffic / Service V0

- **Authorization:** user supplied the targeted traffic/Express request, then explicitly chose thresholds 8/17/18/20 across the entire transfer window, constant vehicle speeds without emergency braking, and permanent per-run consumed-ID exclusion after service completion. Original request: `TRAFFIC_SERVICE_REQUEST.txt`; current rules and exact formulas: `TRAFFIC_SERVICE_V0.md`.
- **Changes:** per-type boarding gate, Express catch zone 7 with body 15, per-ID finite service budgets 160/220/200/150, exact service-end return to CENTER with no Miss, persistent unused budgets on manual departure, and no consumed-ID reuse. Cars remain independent traffic bodies after service end. Both lane schedulers own RNG/timing/pending/active state. Added pure TTC admission and an encounter planner, retaining existing snapshot target selection apart from consumed exclusion.
- **Safety:** TTC uses only vehicle bumper gaps and constant speeds; fixed world-end exit proof is independent of camera/player speed. Continuous checks report a logic error if a safety invariant fails. There is no braking, catch-up position correction, lane changing or velocity mutation of generated vehicles. Occupied lanes can still delay opportunities under this conservative proof.
- **Integration finding:** the first full-run test reached Taxi but not Express because planner fallback slow cars filled the upgrade lane before it cleared. Fixed by preserving upgrade opportunities while awaiting clearance, and allowing voluntary upgrades during the ordinary ride's service warning period. No service/speed values were changed to mask this failure.
- **Verification:** Native Debug and Web Release builds passed. All nine CTest suites passed; the final additional consumed→wait→new-ID assertions also passed in the rebuilt traffic_service executable. A–F scenarios pass; 16 default-config fixed-seed complete races produce 64 Express catches and 64 service ends, with constant trajectories, off-screen births, no consumed-ID catches and no same-lane overlaps. The new suite records 361450 assertions. Logs: `evidence/traffic-service-tests.txt`, `evidence/traffic-service-ctest.log`.
- **Browser:** Edge loaded the production Web build and displayed the new debug fields, lane states, edge entry, input and Miss/recovery. Screenshot: `evidence/traffic-service-web.png`; sampled warning/error logs empty. An initial static JS 404 was traced to sandbox read permissions on the Emscripten output; running the loopback-only preview server at the build's permission level resolved it. No public server or deployment was created. Browser throttling made manual catch timing unreliable, so Express/service-chain acceptance is automated, not claimed as a captured human playtest.
- **Preserved:** 0.25 s transfer, -10/s decay, Run 12, Miss 5, recovery, finish, Deadline and Rank. Score fingerprint changes intentionally invalidate the old infinite-service BEST. Phase 6 rules snapshot is explicitly marked historical and links to the current revision.
- **Playtest pending:** boarding limits, service distances, Express zone, spawn intervals and encounter/visibility lead times. No final balancing claim. Native graphical smoke was not retried after the known GLFW monitor issue. The 1.5–2.5 s encounter range is a priority target, not a guarantee under every occupied-lane state.

## 2026-11-10 — TTC Spawn Safety only

- Scope clarified by the user: defer V0.2 diversity, 9/19/20/22 tuning, transfer diagnostics and Immediate/Prepared opportunities. Reverted the unshipped partial V0.2 edits; boarding limits remain 8/17/18/20. Next-round requirements are recorded in `V0_2_DEFERRED.md`.
- Fixed an overly strict admission branch: a short TTC is no longer rejected merely for being below one second when the leader is guaranteed to retire before the existing clearance is consumed. Fixed-world horizon and one-unit clearance remain; retirement now includes the exact world boundary.
- Added immutable spawn time/position metadata, pure pair inspection, pre-step and post-retirement safety audits, and structured `TRAFFIC SAFETY VIOLATION` evidence. Invalid traffic fails fast after logging; no generated vehicle is slowed, accelerated, teleported or lane-switched.
- Verification: Native Debug and Web Release builds passed; all 10 CTest suites passed. The new suite has 9822 checks, including unsafe long TTC, safe short TTC, three integration partitions, actual Run/Bike/Bus player acceleration, lane-local blocking/release and deliberate fault diagnostics. Existing 16-seed full races still achieve 64 Express catches and 64 service ends without overlap.
- Evidence: `evidence/ttc-safety-tests.txt`, `evidence/ttc-safety-ctest.txt`, `evidence/ttc-protected-files.json`. The deliberate overlap error in the log is expected fault injection, not a failure in ordinary traffic. No new UI elements; browser/manual native graphics were not re-tested in this safety-only revision.
- Exact horizon, formulas and limits: `TTC_SPAWN_SAFETY.md`. Earlier camera-dependent retirement can clear traffic sooner, but admission uses the conservative camera-independent latest exit proof. No final Gameplay tuning claim.

## 2026-11-10 — Gameplay V0.2

- User authorized the deferred next round. Implemented constrained vehicle selection, 9/19/20/22 thresholds, Immediate/Prepared forecasts, retained transfer diagnostics and actual per-type/per-lane admission statistics. Current rules: `GAMEPLAY_V0_2.md`.
- Active and pending cars reserve their type against the opposite lane. Per-lane last type and counts update only on actual admission and reset with a new run. Selection first filters whole-window catch feasibility, off-screen placement and unchanged TTC safety, then prioritizes upgrades/recovery/preparation and randomizes legal alternatives/timing. It no longer aims for equal type frequencies.
- Prepared forecasts replay one real existing, catchable prerequisite vehicle before constructing its next speed tier. Run12 cannot directly board Bus32. Debug classifies existing unconsumed traffic as Immediate or Prepared based on current predictions; this is distinct from an instantaneous in-zone test.
- Transfer diagnostics do not change catch target selection: fallback nearest/rejected vehicles are explicitly diagnostic-only. Independent speed/position tests, player/zone ranges and overlap times stay visible after a CENTER return. The presentation canvas expands to 1360x690 while the road geometry and world visibility remain unchanged. Score fingerprint separates old records from V0.2; Rank/Deadline/finish logic is unchanged.
- Tests: all 11 suites passed. New selection/debug suite passed 6299 assertions. Sixteen default-config scripted races all finished with 48 Express catches and 39 service ends; 3 distinct spawn sequences and 16 actual catches of formerly Prepared vehicle IDs. Every tick checks type reservations, same-lane spawn history, exact counts, constant trajectories, off-screen births and no overlap. TTC and Movement core hash checks passed. Native/Web builds passed without warning/error matches in final build logs.
- Browser: observed Run12 vs Bus32 with speed FAIL (20 > 19), position PASS and final MISS. Saved diagnostic crop `evidence/v02-bus-debug.png`; final panel/column alignment verified in `evidence/v02-overlay-web.png`. Captured console errors were from Zotero extension scripts, not the local game. Native graphical launch was not retried.
- Evidence: `evidence/v02-ctest.txt`, `v02-selection-tests.txt`, `v02-service-tests.txt`, `v02-protected-files.json`. Automated acceptance is not final balancing; user should test Bus timing, opportunity readability, density/waits and Express recovery. Strict type/physics constraints can still delay opportunities after missed progressions.


## 2026-11-10 — V0.3 Movement Feel / intersection semantics

**Problem:** Owner playtest found 0.25s lateral movement too quick, reuse of successfully ridden vehicles, and excessive retained momentum. The proposed 13.33 Bike endpoint was numerically inconsistent; two unmounted 0.40s transfers at -15/s clamp to 12.

**Hypothesis:** 0.40s movement, faster momentum decay and once-used connections increase anticipation. Gating speed at actual first intersection, instead of the entire hypothetical window, retains natural progression with prototype limits 9/19/27/29. Bus's immediate upgrade allowance is about 66.7ms and may still be too narrow.

**Change:** Exact analytic event gate, pre-inheritance event snapshots, consume IDs on successful catch, separate service completion, exit timer, parameter updates and V0.3 diagnostics. The owner explicitly selected contact-time semantics and kept Bus 19. Initial regression exposed endpoint-only forecast placements that produced no Bus progression; generic earlier zone-entry forecasts fix compatibility without changing type priority, weights, reservations, lane scheduler or TTC. No special Run->Bus prohibition was introduced. Old BEST records are isolated by a new mechanics key.

**Result:** 12/12 CTest suites; new 240-check movement-feel contract; sixteen scripted complete runs, 32 Express catches, 39 service ends, 16 Prepared-to-actual catches and 367506 traffic/service assertions. Native build plus 120-frame graphical smoke PASS. Web build and browser start/retry/input/position-failure layout inspection PASS. Actual intersection thresholds/snapshots verified deterministically, not claimed as human playtesting. TTC and LiveTraffic source hashes unchanged. **Pending playtest**: Bus timing, 0.40s anticipation, 15/s momentum, used-ID readability and natural progression. No further tuning performed. Previous source snapshot preserved in ignored `.local/v02-before-v03`; no commit/push of this unconfirmed tuning iteration.


## 2026-11-10 — First Playable delivery preparation

**Problem:** Owner accepted the locally playable version and requested a reproducible GitHub/Pages milestone, with no gameplay redesign.

**Hypothesis:** A clean source snapshot, fixed SDK and official Pages artifact workflow allow browser-only playtesting.

**Change:** Portable local tool overrides, static runtime packaging, pinned Emscripten4.0.23 workflow, concise playtest README, build documentation and machine-path redaction. Existing remote history was retained.

**Result:** Native build/startup and12 CTest suites PASS; Web build and runtime package PASS. Local browser loaded/started, but browser automation stopped before full boarding/finish/storage verification. GitHub rejected Pages activation with HTTP422 due to the Private repository plan limitation. No visibility change, deployment, formal source commit, tag or push. Those steps remain pending the documented blockers; see FIRST_PLAYABLE.md. Gameplay accepted by owner for external playtesting, balance remains provisional.


### First Playable deployment resumed

The owner explicitly authorized Public visibility. GitHub confirmed the change and Pages Source=workflow; the prior plan blocker is resolved. Native12-suite and WebAssembly240/367506/56-check verification passed. Desktop browser control again stopped at URL identification, so full browser verification is assigned to isolated Actions Chromium tests against both the packaged local site and the deployed public URL. No gameplay source changed. Deployment evidence will be recorded after the pipeline completes.

### First Playable published and verified

**Problem:** Complete the authorized public delivery after resolving the private-plan limitation, with reproducible browser evidence.

**Hypothesis:** A fresh Chromium context on an independent runner can verify the packaged runtime and actual public URL without depending on the local desktop browser control limitation.

**Change:** Published source commit `305e9aa` while preserving initial commit `e12e53a`; enabled official Actions Pages deployment; added the verified README play link and retained browser results/screenshots. The `v0.1-playtest` milestone records this verified prototype. No gameplay code or parameters changed.

**Result:** [Actions run 42805243312](https://github.com/kikisukii/Last-Connection/actions/runs/42805243312) passed Build Web, Deploy Pages and Verify public browser demo. The packaged local site and [public Pages URL](https://kikisukii.github.io/Last-Connection/) both loaded real WASM in clean contexts, accepted both arrow directions, caught/followed Bike, returned to CENTER, finished a complete race, displayed rank F and retained Best Time after reload. Finish times were 98.7278 and 98.7611 game seconds; no page errors. Public screenshots were inspected for SUCCESS, rank F and persisted best. See `FIRST_PLAYABLE.md` and its evidence links. Desktop Edge full-run automation remained unavailable due to the URL-identification guard; isolated CI browser verification is not represented as manual playtesting. The owner already accepted this gameplay milestone; further balance remains subject to external playtesting.

## 2026-11-10 — Ride service duration tuning (pending playtest)

**Problem:** Owner playtest reported momentum snowball from long high-speed rides, reducing subsequent transfer significance.

**Hypothesis:** Bike/Bus/Taxi/Express services of 4/3/2/1.5 simulation seconds may preserve the reward of catching faster vehicles while requiring more meaningful connections. This is a playtest hypothesis, not a verified balance outcome.

**Change:** Made per-type `rideServiceDuration` the single tuning source and derived distance as constant speed times duration (80/96/90/90). Kept the existing exhaustion, consumed-ID, CENTER return and decay behavior. Added elapsed/total/remaining ride-time diagnostics alongside remaining distance. Updated distance-based fixtures and score-rule fingerprint access to use the derived value. No cooldown or scheduler/physics/threshold/Rank/Deadline change.

**Result:** Native and Web builds PASS; Native graphical smoke PASS; 13/13 CTest suites PASS. The new 345-check duration contract passes natively and in WASM; WASM movement/traffic/time suites also pass. Existing 16-seed races all finish, with 28 Express catches, 86 service ends and no overlap/velocity/reuse violations. **Pending playtest** for all five owner questions in [RIDE_SERVICE_DURATION.md](RIDE_SERVICE_DURATION.md). Temporary tuning remains local and uncommitted until the owner confirms this iteration; no public Pages update is claimed.

### Ride duration accepted for publication — 2026-11-10

The owner confirmed the current version after playtesting and explicitly requested updating GitHub Pages. This supersedes the pending/local-only status above. The verified duration/diagnostic/test changes are committed as one meaningful iteration and published through the existing Emscripten 4.0.23 Actions workflow, which tests the packaged site and public URL in fresh Chromium contexts. No further gameplay tuning is included. The original `v0.1-playtest` tag is preserved. Deployment success is determined by the linked repository Actions run, not inferred from the push.
