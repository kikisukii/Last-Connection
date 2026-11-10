# Technical notes

> Current implementation: [V0.3 Movement Feel](GAMEPLAY_V0_3.md). First-intersection speed gate, immediate consumption on boarding, 0.40s transfer and 15/s decay supersede earlier phase descriptions below. Historical rules and evidence are retained for provenance.

## Implemented / Phase 1

- C++20 is required by CMake, with compiler extensions disabled.
- raylib 5.5 is fetched at a fixed release tag. No precompiled library is shared across native and WebAssembly targets.
- Native uses GCC and Ninja. Web uses the Emscripten CMake toolchain and raylib's `PLATFORM=Web` with GLFW support.
- Both targets execute the same `frame()` implementation in `src/smoke.cpp`.
- The native platform owns a conventional window loop; the web platform registers a browser-driven `emscripten_set_main_loop` callback, avoiding a blocking browser loop.
- The HTML shell is an execution wrapper, not final UI. The static preview server binds to 127.0.0.1 only.
- Build, dependency and temporary paths stay inside the original project directory; toolchains live in D: directories. Process-local environment changes avoid a machine-wide PATH edit.

## Implemented / Phase 2

`Movement` is a small pure C++ state holder with `requestTransfer(direction)` and `update(seconds)`. It does not include raylib and is tested without a window. `src/main.cpp` polls input, advances the model at 120 Hz and renders a following camera. The entire current gameplay configuration is in `include/gameplay_config.hpp`.

Phase 2 forward movement was `position += normalRunSpeed * dt`; Phase 4 uses the current mutable forward speed. Lateral movement uses `u = elapsed / duration`, `s(u) = 3u² - 2u³`, and `lateral = start + distance * s(u)`. Its derivative gives velocity `distance * 6u(1-u) / duration`; the second derivative gives acceleration `distance * (6-12u) / duration²`. Zero initial/final lateral velocity avoids snapping; acceleration is discontinuous at the endpoints. Duration and spacing determine acceleration. This is a prescribed movement curve, not an independently integrated force simulation.

The logical lane remains the source until completion. Transfer requests outside -1/+1, beyond the lane boundary or during an active transfer are rejected. A time tolerance of a few floating-point ULPs prevents a 30-tick transfer taking 31 ticks due to rounding.

The adapter drains raylib's PRESS-event queue once per rendered frame. This preserves short taps even if release happens before the next frame, ignores GLFW key-repeat events, and prevents input from accumulating across a transfer. Opposite direction events within one frame cancel. The core has no input queue.

`FrameClock` accepts at most 1/60 s per rendered frame, then returns the number of 1/120 s physics steps. The former 0.1 s catch-up cap allowed a fast short vehicle to appear mostly inside the viewport in its first visible frame. Excess stall time is dropped for the entire simulation, not accumulated for later catch-up and not applied separately to vehicles. At low FPS, this intentionally slows simulation relative to wall time. In Phase 6 the user explicitly chose this same **simulation time** for scores/deadline. Browser background throttling can therefore slow or suspend motion and timing together; no wall-clock score or separate pause control is implemented.

`RoadView` provides the shared center-to-bumper screen projection for drawing and entry tests. The front bumper reaches the road's bottom before the center or rear bumper. Headless entry tests drive the same `FrameClock`, `Movement`, `RandomVehicleStream` and `RoadView` used by the app, with entry deliberately between frame boundaries. The prior frame budget fails the first-visible-fraction regression; the new default keeps first visibility connected to the bottom edge and below 12% of body length across all four current types, including long-frame scenarios. This is simulated frame-delta coverage, not measured browser performance.

The 34 deterministic checks cover initial state, forward motion during transfers, boundaries, no direct crossing, mid-transfer rejection, analytic trajectory, zero endpoint velocity, duration, different timestep partitions, configuration changes and invalid deltas. Browser and native render smoke checks supplement these logic tests; they do not evaluate game feel.

## Later / Not implemented

Broader browser verification, deployment and portfolio polish remain later work. Continuous catch/riding, momentum decay/miss/recovery and local Time Attack are implemented in Phases 4–6 below.

## Implemented / Phase 3

**Historical Phase 3 runtime:** `RandomVehicleStream` draws types, lanes and intervals for a constant-speed camera. Phase 4 uses `LiveTraffic`; both earlier streams remain regression fixtures only.

`std::mt19937` is seeded once per run. Its specified 32-bit outputs are mapped explicitly to [0,1) so native and Web need not depend on library-specific `std::discrete_distribution` mappings. Type selection uses cumulative normalized weights (initial 1/1/1/1); lane selection uses a configurable left-lane probability (initial 0.5). A time-based seed is the default; config can force a displayed seed for replay. This is gameplay pseudorandomness, not cryptographic randomness.

`sample(time)` advances random generation to a nondecreasing simulation time. The first request is at the configured 1 s; after each admitted entry the next interval is sampled in [2,4) s. Admission retains the chosen type and lane, delaying rather than rerolling them. Starting the next request after actual entry prevents unlimited pending queues. The latest planned vehicle in each lane supplies the existing clearance constraint. Older same-lane bodies cannot still be ahead on screen after that leader has exited, because every earlier admission already preserves order while visible.

Active plans are pruned at generation events and at the sampled time. Only active/future plans plus two latest-lane records are retained, while IDs and aggregate draw counters continue increasing. Positions remain analytic at configured speeds. Same seed/config gives the same choices and positions under different frame partitions; rewinding requires a new stream instance.

Tests retain the original overlap regression and add: empty startup, continued generation beyond eight cars, same-seed replay, different seeds, unchanged probabilities with delays, configurable zero-weight types, continuous interval clearance for 64 crowded seeds, long-run cleanup and invalid-time/config rejection. A fixed 33,111-draw sample is close to 25% per type and 50% per lane. This does not imply exact ratios in a short run or final tuning.

### Historical fixed-sequence fixture

`VehicleStream` validates a `GameplayConfig`, plans safe screen-entry times once, then samples the authored sequence at a supplied simulation time. Each vehicle has a stable sequence-index ID, type, lane, world-space body-center position, speed, length and entry delay. Every position is evaluated as `plannedInitialPosition + speed * elapsedSeconds`; sampling is stateless and independent of rendering order, camera culling or earlier samples.

The renderer samples the player's simulation clock, projects `vehiclePosition - playerPosition` through the same forward world-unit scale as the road, and clips bodies to the road viewport. All vehicles exist from the start; no random spawn timers, wrapping or recycling are used. Off-screen vehicles remain in the sequence. All currently move forward along their initial outer lane.

The debug overlay reports the nearest vehicle in forward distance, its type/lane, absolute speed, relative speed (`vehicleSpeed - playerSpeed`), center position and body length. This is not a catch-target selection rule. The current player simulation never queries the stream, so geometric overlap cannot accidentally mount a vehicle or affect speed.

The vehicle suite now has 242 checks covering stable IDs, outer-lane invariants, configured speeds/lengths, deterministic replay, finite sequences, expected encounter positions, independent player speed, whole-interval visible clearance and invalid configuration/time rejection. The next phase must introduce a separately testable swept catch detector; do not replace it with body overlap.

### Visible traffic clearance

User requirement: no same-lane vehicle-body overlap inside the road viewport; off-screen behavior need not simulate traffic collisions. To retain the original speed-based prediction mechanic, the fix delays entry instead of slowing vehicles on screen.

Let `u = vehicleSpeed - normalRunSpeed`, `W = visibleAheadDistance + visibleBehindDistance`, `L` be vehicle length and `g` the minimum bumper gap. Entry is when the front bumper reaches the rear viewport edge; exit is when the rear bumper passes its forward edge. All current `u` values are positive. Vehicles are processed in authored entry-time order, with stable IDs for ties. Against each earlier same-lane vehicle, the follower's entry time must satisfy both:

1. `followerEntry >= leaderEntry + (leaderLength + g) / leaderRelativeSpeed` (space at entry).
2. `followerEntry >= leaderExit - (W - g) / followerRelativeSpeed` (no catch before leader leaves).

Take the maximum of those bounds and the requested entry time. Clearance is linear in time, so checking its two endpoints protects the entire interval while both bodies are visible. After calculating the delay, shift the follower's initial position back by `relativeSpeed * delay`. Its world speed stays unchanged; there is no on-screen repositioning or braking.

Regression fixture: at 20.5 s, the original right-lane bike center was 266 and Taxi center 262.5. Their bodies overlapped by 0.5 units inside the visible forward edge. The current planner delays Taxi #7 by 0.172931 s and keeps at least 1 unit of bumper clearance throughout shared visibility. Tests also exercise crowded entries, reversed IDs, changed view lengths and zero-gap tuning, and allow passing once offscreen.

**Phase boundary:** this plan is valid for the current constant-speed following camera. Riding, deceleration and recovery will change camera motion and may expose previously off-screen intersections. Update the admission model and its tests before enabling those mechanics. Traffic at or below the current camera speed is explicitly rejected instead of silently producing an invalid plan.

## Phase 4 implementation baseline (Phase 5 changes below)

`predictCatch` is pure C++ without rendering or mutable state. Let `r = playerPosition - vehiclePosition`, `u = playerSpeed - vehicleSpeed`, `h = catchZoneLength / 2`, and `T = transferDuration`. Solve `-h <= r + u*t <= h` and intersect the solution with `[0,T]`. For `u == 0`, the entire interval qualifies exactly when `abs(r) <= h`; otherwise sort the two boundary roots. Enter/exit boundaries and the speed threshold are inclusive. `abs(u) <= maxTransferRelativeSpeed` is checked separately from geometry. This finds brief intersections even if both sampled endpoints lie outside.

`Game` owns movement, an optional attached vehicle ID, the transfer-start snapshot and a scheduled first-intersection time. A valid adjacent-lane request releases an old attachment while retaining velocity. It evaluates all currently retained target-lane vehicles, selecting earliest allowed entry (then lowest ID). A rejected geometric candidate is retained for diagnosis if there is no eligible target. Invalid/mid-transfer input cannot release attachment. New traffic born during a transfer is outside the view and not added retrospectively to that start snapshot.

The user chose early longitudinal following. `Game::update(dt)` splits integration at the intersection: integrate old velocity to the event, set vehicle velocity without moving the player, then integrate the remainder. The original lateral curve continues. At arrival, attached becomes Success/Riding; a transfer to CENTER is departure rather than Miss. Other arrivals report no intersection or relative-speed rejection. Remaining unmounted velocity stays constant until Phase 5 implements decay/recovery. There is no death, penalty or reset in this phase.

The main loop consumes input against the current traffic snapshot. Each fixed step updates `Game`, advances all vehicle world positions by their constant speeds, then admits/removes traffic using the player's actual end position and speed. Rendering is read-only. Matching speed after catch keeps the player/vehicle offset constant, including a catch between fixed-step boundaries. The caught vehicle is retained because its body continues to intersect the actual view; there is no old constant-camera exit deadline.

`LiveTraffic` replaces the fixed-camera planner. Candidate bodies begin fully outside the relevant viewport edge plus a configurable 0.02-unit margin. Faster vehicles approach from behind; slower ones approach from ahead. For every retained same-lane vehicle, a candidate must have the configured bumper gap and a nonclosing speed order (behind means no faster; ahead means no slower). Linear constant world velocities then guarantee the pair cannot intersect, independently of camera movement. Retiring an off-screen body is allowed by the user's constraint; it cannot reappear with the same ID. This deliberately conservative proof permits longer spawn delays than the old constant-camera formula.

One pending candidate per lane bounds storage and lets the opposite lane continue. Request opportunities use the original interval range but now recur independently of admission; occupied pending slots keep their existing type. Draws use the same explicit mt19937 mapping and equal weights. Replay requires the same seed, fixed-step player trajectory and input timing, not only the seed. New unseen cars moving away from the view or matching camera speed are retired; seen cars retire only after exiting. No visible car is displaced or slowed. Same-speed on-screen traffic may stay alongside and block that lane; the current no-decay phase can therefore have sparse traffic after mounting.

The debug overlay exposes player/target speeds, start relative speed, player/target positions, transfer progress, target ID, state/outcome, catch-zone length and predicted enter/exit times. Target position is extrapolated from the recorded attempt; the record persists until the next transfer, even if that vehicle has since retired. Inner bright rectangles show catch geometry; the attached one is green.

Tests add continuous-window/state contracts and 32 seeded live-camera stress runs. They check exact end-window events despite floating-point accumulation, split integration versus one large step, same-speed and overtaking catches, threshold rejection, departure, off-screen births, constant vehicle trajectories, pair clearance through repeated camera speeds 12/20/45/60/32/20/12, seeded replay, probability samples and a real generated bike retained through over 30 seconds of riding. Live entry tests cover both camera edges and simulated low render rates. Historical suites stay intact. These tests establish implementation behavior, not final game feel.

## Implemented / Phase 5

`forward_motion.hpp/.cpp` defines the shared pure speed integrator. Above normal, acceleration is `-normalDeceleration`; below normal, it is `+recoveryAcceleration`. Compute time to equilibrium, integrate `v*t + 0.5*a*t²` only to that instant, then integrate the rest at normal speed. Velocity clamps exactly at equilibrium. Zero configured acceleration disables that branch. This makes both speed and distance independent of timestep partitioning. `Movement::update(dt, relaxSpeed)` uses that integrator when unmounted; ordinary constant-speed motion remains available for riding and historical tests.

`Game::update` now splits at catch AND transfer completion, handling catch before completion if both coincide. Unmounted segments relax speed; following/riding segments do not. At an unsuccessful outer-lane arrival, record before/after speed and apply `min(before, max(minimumForwardSpeed, before - missPenalty))` once. A CENTER arrival has no penalty. Remaining time in the same update uses the post-penalty trajectory, so a large update and many small updates agree. Rejected input never releases an attachment or deducts speed. The floor ensures forward motion after repeated misses without making 12 a minimum; the outer min prevents increasing unusually slow configured vehicle speeds.

`predictRecoveringCatch` replaces the linear predictor in live gameplay. It divides the transfer window at the time velocity reaches equilibrium (at most two pieces). On each piece, relative position is quadratic, `r + u*t + 0.5*a*t²`, and relative speed is linear. Solve both catch-zone boundary equations and the two relative-speed limit crossings. Sort those roots with the piece endpoints; geometry and speed eligibility cannot change inside any resulting open interval, so test each midpoint plus every boundary. Separate endpoint checks preserve tangent contacts. Stable quadratic roots reduce cancellation; a small numerical tolerance handles boundary arithmetic. Select the first interval satisfying geometry AND speed; if geometry exists but no interval is safe, keep the first geometric interval for the debug rejection. No time sampling is used for the actual decision.

This deliberately evaluates relative speed at the potential catch event rather than at transfer initiation. A recovering player may enter the safe speed range while already overlapping a catch zone. A decelerating player may become ineligible before geometrically meeting a faster vehicle. The same forward integrator supplies prediction and actual movement, keeping event position continuous. The debug delta now means event-relative speed. Predicted interval exit still describes the hypothetical unmounted path; after catching, velocity follows the vehicle instead.

New parameters are centralized: deceleration 10, recovery acceleration 5, penalty 5 and minimum forward speed 1. Only deceleration/penalty came with numerical values in the brief; recovery/floor are explicitly provisional implementation defaults. Leaving starts decay immediately, including the transfer back to CENTER. This makes transfer duration consume real momentum: a Bus at 32 leaves at 32, then reaches 29.5 after 0.25 s. Riding remains at exactly 32.

The recovery suite checks analytic distance/speed examples, no overshoot, exact penalty timing, repeated misses/floor, normal departure, high-speed miss → slower-Bus recovery, curved/tangent/equilibrium-crossing catches, event-speed gating, and no snap when catching during recovery. A dense numerical oracle covers representative speed/position combinations independently of root selection. Eight 100-second integrated fixed-seed runs produced 680 catches and 440 misses with continued forward progress and no retained same-lane body overlaps. Live entry tests also cover camera speed 1: current Taxi geometry can expose up to about 15% of its body in a two-tick render interval at that relative speed, while still entering through the bottom; the normal-speed 12% regression remains intact. These are automated engineering checks, not human balance tests.

## Implemented / Phase 6

`TimeAttack` owns `Game`, `LiveTraffic`, race configuration and optional finish time. Keeping finish/rank outside `Game` preserves the earlier motion/catch test fixtures as unlimited simulations. Each live tick advances gameplay and traffic on the same accepted simulation time. The race clock reads `Game`'s elapsed time; no independent wall-clock counter can drift from movement. This follows the user's explicit clock choice, including the existing low-FPS slowdown. Loading/menu time does not advance the race. Background throttling can slow or stop the simulation; there is no separate wall-clock deadline or pause mechanism.

When the predicted end of an update crosses the airport, `TimeAttack` locates the first crossing by a bounded 60-iteration bisection using copies of the actual `Game` integrator. Forward speed remains positive, so position is monotone even through catch and miss events. This avoids using a constant-speed interpolation through acceleration or an in-tick catch. Only the time up to the crossing advances the real game and traffic. Further updates/input are ignored after finish. The displayed hundredths are rounded; classification uses the unrounded finish with a 1 ns numerical tolerance around inclusive cutoffs.

`deadline` only controls late status and D/F classification. The user chose to keep moving after the deadline and report actual arrival time. Initial S/A/B/C limits must be strictly increasing and below deadline; D extends to deadline, F is later. Configuration rejects nonfinite/nonpositive distance/deadline and invalid ordering. A no-input default run has exact theoretical finish 1200/12 = 100 seconds and rank F. The airport is a simple line/progress readout, not a completed art environment.

The UI has a pre-run state and a frozen result state. A menu press starts/retries without performing an unintended lane transfer and discards that menu frame's delta. Retry reconstructs `Game` and `LiveTraffic`, clears finish/result flags and replaces `FrameClock`, while retaining the loaded best record. Random mode chooses a new seed; fixed-seed mode repeats its configured seed.

`best_time_storage.cpp` is the platform adapter: native uses a bounded file next to the executable; Web uses localStorage through Emscripten JS bindings. Pure encode/decode functions accept only finite positive times with a matching `scoreRulesKey` and no trailing junk. The key includes gameplay, race, traffic and clock parameters, excludes the unused historical fixture and excludes each ordinary random seed. It includes the configured seed in fixed mode. One saved record is retained; changed tuning is ignored on load and replaced after the next completed run. Best includes late completions, improves only when a faster completion occurs, and survives in-session retry. I/O failure preserves an in-memory record and exposes a session-only message. Native/browser/origin records are independent; no backend or cloud storage exists.

The race test suite covers 56 checks: all inclusive rank boundaries, hundredth rollover, a fractional-tick finish under simulated 144/60/30/10 FPS and 500 ms stalls, post-finish freezing/input rejection, complete retry reset, continued late running, exact-deadline D despite floating-point accumulation, a recovery-curve finish with an independently solved quadratic time, a real generated-Bike riding finish, and corrupt/config-mismatched best records. Existing seven suites still run. Automated evidence is separate from user playtesting and deployment readiness.

## Official implementation references

- [Emscripten installation](https://emscripten.org/docs/getting_started/downloads.html)
- [raylib web build guide](https://github.com/raysan5/raylib/wiki/Working-for-Web-%28HTML5%29)
- [w64devkit releases](https://github.com/skeeto/w64devkit/releases)
