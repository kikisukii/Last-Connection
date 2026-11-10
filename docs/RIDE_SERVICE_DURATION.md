# V0.3 ride service duration tuning — accepted for deployment

The owner's new playtest found long high-speed rides could advance too much of the route and reduce the value of subsequent transfers. This iteration changes only the ride service budget and its diagnostics. All values below are **prototype tuning values, not final balance**.

| Vehicle | Unchanged speed | rideServiceDuration | Derived service distance |
|---|---:|---:|---:|
| Bike | 20 | 4.0 s | 80 |
| Bus | 32 | 3.0 s | 96 |
| Taxi | 45 | 2.0 s | 90 |
| Express | 60 | 1.5 s | 90 |

`VehicleSpec::rideServiceDuration` in `include/gameplay_config.hpp` is the sole service tuning input. `rideServiceDistance()` derives `speed * rideServiceDuration`; there is no second writable distance value to keep synchronized. Actual attachment initializes the existing distance budget with the attached vehicle's constant speed times that duration. The timer uses simulation time, so low frame rates/background throttling can make wall-clock time longer.

Service starts at the actual successful Catch Zone intersection, including the remainder of lateral transfer. It does not start at spawn, transfer request or lateral completion. Early manual departure ends the ride; it cannot restore the used ID or its budget. The existing event integrator splits exactly at distance exhaustion.

At exhaustion, the existing rules remain: release at current speed, no Miss or penalty, mark service completed, start the normal 0.40 s return to CENTER and decay at 15/s. Express therefore releases at 60 and reaches CENTER at 54 after that return. The old vehicle continues its constant trajectory until normal retirement and stays ineligible for this player. The planner can arrange another legal connection normally. No cooldown, forced delay, type prohibition or random penalty was added.

## Debug

The middle panel shows current vehicle ID/type/speed, `Ride time: elapsed / duration`, `Time remaining`, `Distance remaining`, and the consumed/completed information. All timer numbers derive from the same remaining distance used by gameplay. Unattached fields show `--` rather than pretending a ride is still active. The BEST label moved below the last-spawn labels to leave room; score/rank presentation semantics are unchanged.

The existing best-record fingerprint still includes derived service distance, so these shorter-service runs do not compare against older long-service records. Rank thresholds, Deadline, finish distance and score calculation are unchanged.

## Verification

- Native build and 120-frame graphical startup/exit smoke PASS.
- 13/13 CTest suites PASS.
- New `ride_duration_contract`: 345 checks. Every type reaches its real catch through the natural ladder, starts its timer before lateral completion, stays attached just before the deadline, expires at its configured time/distance, preserves speed with no penalty, returns to CENTER with decay, rejects the completed ID, and accepts a fresh eligible ID. Partitions include 120 Hz, 60 Hz, 0.137 s and a single update spanning service end. Custom duration and invalid input checks also pass.
- Web build PASS, Emscripten 4.0.23. The same C++ tests compiled to WASM pass: movement feel 240, traffic/service 467630, time attack 56, ride duration 345 checks.
- Existing live-traffic stress: 16 fixed-seed complete races; 28 Express catches, 86 service ends, 35 actual catches of previously Prepared IDs, 5 distinct spawn sequences. Constant speeds, no overlaps, consumed IDs, independent lanes and legal diversity remain checked. Express naturally connects to a new Taxi after service in the dedicated continuation scenario. These counts are automated regression evidence, not a judgement of balance.
- Local browser: loaded the final Web build, started with keyboard input, and visually checked the updated panel with no overlapping BEST text; no captured console warnings/errors. [Overlay screenshot](evidence/ride-duration-overlay.png). The embedded browser was heavily throttled, so the four service durations are verified by deterministic Native/WASM tests, not claimed as manually timed browser rides.
- Movement, catch detection, traffic scheduler, encounter planner and TTC safety source files remain identical to the accepted baseline. Protected config parameters outside the service values remain unchanged.

## Owner playtest

1. Does Express's 1.5 s service still reward a successful catch?
2. Do high-speed opportunities still feel like effortless consecutive rides?
3. Are consecutive high-speed transfers still too easy?
4. Are Bike 4 s and Bus 3 s too long?
5. Do connections remain meaningful across the 1200-unit route?

**Accepted for deployment on 2026-11-10.** After actual playtesting, the owner confirmed this version and requested a GitHub Pages update. This is approval of the current prototype iteration, not a claim of final balance. The existing Actions workflow builds and verifies the release on push to main. The `v0.1-playtest` tag remains on the historical first-playable baseline; it is not moved or overwritten.
