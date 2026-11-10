# LAST CONNECTION — Design

> Current implementation: [V0.3 Movement Feel](GAMEPLAY_V0_3.md). First-intersection speed gate, immediate consumption on boarding, 0.40s transfer and 15/s decay supersede earlier phase descriptions below. Historical rules and evidence are retained for provenance.

## Design Goal

Minimalist two-button time-attack game. A single continuous route leads to the airport. The core decisions concern **when to transfer**, not which route to choose. The user decides mechanics and final parameters.

## Core Mechanic

Predict the future position of a faster-moving vehicle during a lateral transfer. One LEFT or RIGHT press requests one adjacent lane change. LEFT ↔ RIGHT always passes through CENTER. Forward and lateral movement are independent. There is no jump or direct speed control.

## Speed System (implemented Phase 5)

- Normal run speed: initial 12 units/s, an equilibrium rather than a minimum.
- Speeds above normal decay toward normal; speeds below it recover gradually.
- Riding matches the vehicle's speed.
- Initial decay: 10 units/s²; miss penalty: 5 units/s.
- Initial recovery: 5 units/s²; positive speed floor for repeated misses: 1 unit/s. Neither is final tuning. The floor never raises an already-slower customized vehicle's speed.
- Unmounted motion relaxes toward 12 during lateral transfers and in every lane. Riding and early following remain locked to vehicle speed. Integration clamps exactly at equilibrium without overshooting it.
- Bike / bus / taxi / express initial speeds: 20 / 32 / 45 / 60 units/s.
- All values are prototype parameters, not a completed balance pass.

## Phase 2 behavior (historical baseline)

- Three lanes at lateral coordinates -4 / 0 / +4; player starts in CENTER.
- Constant forward speed of 12 units/s in every lane for this vehicle-free sandbox.
- Adjacent transfer duration starts at 0.25 s, using a cubic smoothstep velocity profile. Lane spacing, duration and forward speed are editable in `include/gameplay_config.hpp`.
- No mid-transfer reversal or buffered follow-up input. Opposite press events in one rendered frame cancel. Key-repeat events are ignored. These are provisional input policies, not new gameplay features or final tuning decisions.
- Lateral acceleration and deceleration are derived from distance and duration, rather than independently tuned forces. The player settles at the target lane with zero lateral velocity. This provides a timed inertial-feeling trajectory without a physics engine; it is not a free-body inertia simulation.
- The debug overlay shows speed, normal speed, source lane, target lane, state, progress, forward position, lateral velocity/acceleration and simulation time. Vehicle fields are not applicable yet.

## Vehicle Catch Detection (implemented Phase 4)

Vehicles have a physical length and a shorter gameplay catch zone. Transfer initiation records forward positions, velocities and duration. The detector must test the trajectory against moving catch intervals throughout the transfer window, including intersections between simulation samples. End-frame overlap or arbitrary model contact alone is insufficient.

A successful transfer reaches the adjacent lane, has a catch-zone intersection during its window, and satisfies a configurable relative-speed threshold. The detector solves a continuous relative-position interval, including events between simulation samples. Earliest eligible intersection wins; stable ID breaks ties. Boundary contact is inclusive. A geometric candidate above the threshold produces a distinct debug miss reason.

The user explicitly chose: “交会时开始跟随车辆速度，横移继续完成”. Integration splits at the first eligible intersection, immediately matches longitudinal speed and preserves position. Lateral motion continues until the original end time; then the state becomes Riding. The stored forward offset stays constant. If already in the zone at transfer start, intersection time is zero; no additional lateral-progress gate is imposed. Standing in a side lane alone does not mount passing vehicles.

Initial centered catch-zone lengths are Bike 2 / Bus 7 / Taxi 3.5 / Express 11. Initial relative-speed limit is 30 units/s. These are editable, unbalanced prototype values. At speed 12, this permits Bike/Bus and rejects Taxi/Express until more momentum is available. Phase 5 predicts the accelerating/decelerating forward trajectory and checks the threshold at the candidate intersection, rather than retaining the transfer-start speed check. A limit crossing during an overlap can create a later eligible catch.

## Failure (implemented Phase 5)

A miss causes temporary momentum loss, never death, restart or a position reset. Remaining momentum permits recovery onto another vehicle. Lower speeds recover toward the normal equilibrium.

Apply the penalty once at the end of an unsuccessful transfer toward LEFT/RIGHT. Both no intersection and excessive relative speed count as misses. Leaving toward CENTER is ordinary departure, with gradual decay but no penalty. Invalid input, further update ticks and standing in a traffic lane cannot retrigger a penalty. Repeated misses clamp at the configured positive floor. A normal-speed miss is initially 12 → 7 → gradually 12; a high-speed miss keeps its remaining velocity and can catch a slower vehicle before equilibrium.

## Phase 3 behavior (historical; live changes below)

- The user explicitly replaced the original fixed-sequence requirement with random traffic and equal initial probabilities. Live traffic draws Bike, Bus, Taxi and Express independently at 25% each; LEFT and RIGHT are initially 50% each. Center remains traffic-free.
- Body lengths and speeds use the brief's initial values. Positions, lanes and all values remain editable in `GameplayConfig`.
- All vehicles advance on the same simulation clock as the player. Default refresh/relaunch uses a new seed; an optional fixed seed reproduces a run for debugging.
- Rectangular bodies have visible labels and lengths; there are no catch zones or riding transitions yet.
- Generation continues beyond the old eight-car limit. Initial timing is 1 second to first requested entry, then a uniformly random 2–4 seconds after each actual entry. This is editable prototype timing, not a balance conclusion. Exited vehicles are cleaned up.
- Phase 2 movement was confirmed by the user. The user then reported a larger vehicle passing through a bike and specified that vehicle bodies must not collide in the visible canvas; off-screen behavior is not important.
- Same-lane screen-entry timing includes a configurable bumper gap (initially 1 world unit). Conflicting traffic enters later from behind the camera, retaining its configured type and speed. It is not rerolled to find an easier-to-fit type. No global traffic simulation or on-screen braking is added.
- This visibility-based admission model currently assumes the Phase 3 constant-speed camera. Revisit it before introducing riding or any other player-speed change.
- User reported that the yellow Taxi seemed to appear suddenly near the bottom, while Bike entry looked normal. A reproduced long-frame path showed the short/fast Taxi could expose about 65% of its body on its first visible draw under the old frame budget. The current technical frame budget is 1/60 s for the whole simulation; car speeds/probabilities are unchanged. Low-FPS play therefore runs in simulation slow motion. This is a provisional presentation tradeoff, not a finalized time-attack clock.

## Phase 4 traffic with a moving camera

- Live admission now uses actual player position and speed. Faster vehicles start beyond the bottom; slower ones start beyond the top and move downward relative to the overtaking player. All vehicles still move forward in world space.
- Keep the user's equal type weights and no visible body overlap. A retained same-lane pair must have bumper clearance and nonclosing speed order. Conflicting types wait off-screen; visible bodies are never slowed or repositioned.
- Requests occur every configured 2–4 simulation seconds, with one pending slot per lane so one blocked lane does not stop the other. The type is drawn only when that slot is empty. Actual visible counts may differ from random draw proportions.
- This is conservative admission, not traffic physics. Seen vehicles retire after exiting the actual view; unseen co-moving or retreating vehicles retire off-screen. Same-speed visible vehicles remain alongside the camera and can block that lane's pending request while riding. From Phase 5, departure decay changes relative motion and lets such traffic move away naturally.
- The user confirmed the preceding short-Taxi entry fix: “现在OK了”. The shared 1/60 s frame budget remains, with its documented low-FPS slow-motion tradeoff.

## Time Attack and replay (implemented Phase 6)

A fixed airport finish, actual finish time, deadline, best time and S/A/B/C/D/F ranks. F is MISSED FLIGHT. Initial finish distance is 1200; S/A/B/C upper times are 30/40/50/65 seconds, D extends through the 90-second deadline. All are configurable provisional values requiring user playtesting. Keeping D's upper bound equal to the deadline avoids contradictory D/F definitions.

The user explicitly chose simulation-time scoring: timer and vehicle motion slow together under the existing capped frame budget. The user also chose to continue after the deadline, reaching the airport before showing actual finish time and F. A late warning therefore does not terminate the run. Finish/deadline thresholds are inclusive; the finish can occur between fixed-step samples. A positive speed floor guarantees eventual forward arrival even after repeated misses.

The start screen waits for a click or LEFT/RIGHT; this action starts the run without also changing lane. The result freezes simulation, shows finish/rank/best, and offers RUN AGAIN using the same menu inputs. Retry resets lane, speed, traffic, timer, transfer and frame accumulator, retaining best time. It normally chooses a fresh random seed. No extra movement control was added.

Best time is local to a browser origin or native build directory, and tied to gameplay configuration. A late finish can be the first best record; a subsequent faster arrival replaces it. Corrupt or mismatched records are ignored; unavailable storage falls back to session-only best. Records are not synchronized or uploaded. Comparing random runs is part of the currently authorized random-traffic prototype; final fairness/balance is not claimed.

## Phase gates

0. Inspect environment and install only missing tools on D:.
1. Run the same minimal native and web program.
2. Three lanes, player, automatic forward motion and timed adjacent transfers; no vehicles.
3. Deterministic vehicle streams using simple geometry.
4. Continuous transfer-window catch detection.
5. Miss and recovery.
6. Time attack.
7. Browser gameplay verification.
8. GitHub Pages deployment.
9. Portfolio presentation.

Random vehicle spawning is now explicitly authorized. No branching routes, combat, inventory, backend, accounts, procedural level generation or large physics engine are authorized.
