# V0.3 Movement Feel

Owner accepted this version as First Playable on 2026-11-10; final balance remains open. This is the current implementation, following the owner's clarification on 2026-11-10. It supersedes V0.2's whole-window boarding speed gate and service-exhaustion-only consumption. These are prototype tuning values, not final balance values. Automated verification does not establish game feel.

## Parameters and exact calculation

| Parameter | V0.3 |
|---|---:|
| Adjacent lateral transfer | 0.40 s |
| Normal run speed | 12 |
| Unmounted deceleration above normal | 15/s |
| Recovery below normal | 5/s |
| Miss penalty at failed transfer completion | 5 |
| Bike / Bus / Taxi / Express boarding limits | 9 / 19 / 27 / 29 |

The forward integrator is analytic and splits at equilibrium. Above normal, `v(t) = max(12, v0 - 15*t)`; below normal, `v(t) = min(12, v0 + 5*t)`. No approximate intermediate speeds are used. Catch events split the simulation step before the player inherits the actual target speed.

Bike departure is `20 - 15*0.40 = 14`, not 13.33. From 20 to 12 takes `8/15 = 0.533333... s`. The second transfer begins at 14 and reaches 12 after `2/15 = 0.133333... s`; two complete unmounted transfers therefore end at **12**.

| Connection | Speed at second transfer start | Unmounted speed through that window | Relative difference at contact | Limit |
|---|---:|---|---|---:|
| Run → Bike | 12 | 12 | 8 | 9 |
| Bike → Bus | 14 | 14 → 12 | 18 → 20 | 19 |
| Bus → Taxi | 26 | 26 → 20 | 19 → 25 | 27 |
| Taxi → Express | 39 | 39 → 33 | 21 → 27 | 29 |

With no pause in CENTER, Bike → Bus succeeds if first contact is within `1/15 = 0.066666... s` after the second transfer starts (inclusive). A CENTER pause consumes that same momentum allowance. This is narrow and must be playtested; **Bus remains 19**. A catch at 0.04 s records player 13.4, difference 18.6, PASS. A catch at 0.12 s records 12.2, difference 19.8, FAIL. At 0.17 s the unmounted player is already 12, not 13.8.

Direct Run → Bus/Taxi/Express differences are 20/33/48, exceeding 19/27/29. After Bike departure, Taxi/Express differences start at 31/46; after Bus departure, Express starts at 34. No lane/type-specific skip prohibition or hardcoded player-speed cutoff is added. Changing the Bus limit to 20 automatically permits a Run12 intersection; a deterministic test verifies that configurability.

## Boarding and consumption

1. A valid adjacent input snapshots existing unused vehicles in that lane. New spawns cannot join the attempt. Inputs during lateral motion are discarded; no cancel, reverse, queue, auto-mount, or two-lane move is added.
2. Solve the complete unmounted 0.40 s trajectory against each moving Catch Zone, including equilibrium clamp and tangent/end contacts.
3. At that vehicle's **first geometric intersection**, evaluate `abs(playerSpeedAtIntersection - vehicle.speed) <= target.maxBoardingRelativeSpeed` (numerical tolerance 1e-10 only). Starting inside the zone means contact at t=0. A rejected first contact cannot be rescued by waiting inside the same overlapping zone for a later speed pass.
4. Select the earliest eligible target; ID breaks ties. The earliest rejected car or nearest unused car may be retained solely for diagnostics.
5. At actual contact, record time and pre-attachment speed. On success, immediately inherit constant vehicle speed, mark its ID consumed, start its service budget, and continue the lateral move. Subsequent hypothetical unmounted speeds do not veto the catch.
6. Consumption is membership in the per-player ID ledger; service completion is a separate remaining-distance check. A currently ridden consumed ID continues its current service normally. Manual exit never restores eligibility. Failed attempts create no ledger entry.
7. Service exhaustion releases at current speed, begins a 0.40 s return to CENTER and normal decay, with no Miss. The vehicle remains in traffic until normal retirement. No replacement is forced.
8. A failed outer-lane transfer applies the unchanged penalty only after its natural 0.40 s speed evolution. Example Bus rejection: end speed12 → penalty5 → speed7, followed by recovery5/s.

`vehicleConsumed(id)` is the explicit new name. `serviceConsumed(id)` remains a compatibility alias for existing planner call sites and now means the same once-used eligibility rule. `serviceCompleted(id)` specifically means exhausted service. Current and last-used IDs are distinct from completed service.

## Encounter forecast compatibility

The original spatial forecast placed the vehicle center at the predicted transfer endpoint. Under the new decay/limits, this made Bus contacts too late and eliminated Prepared Bus opportunities: the initial integration stress test boarded only Bikes.

The planner now preserves an original viable placement; if that placement fails the shared boarding predictor, it also considers zone-entry positions at 0%, 25%, 50%, 75% and 100% of the transfer. Every candidate uses the same analytic trajectory and configurable intersection gate. This is generic spatial prediction, with no Bus-specific cutoff, type override, weight change, or forced connection. Existing off-screen birth, observation lead, TTC, per-lane scheduling, type reservations and opportunity priority filters still apply. A zero-offset candidate makes an early Bus contact possible; it does not automatically issue player input.

The LiveTraffic type selection/scheduler and traffic safety implementation are byte-identical to the V0.2 snapshot. Planner position forecasts necessarily change to match the newly authorized catch semantics; spawn counts/timings can therefore differ under the same seed. No generated vehicle speed is changed.

## Debug and how to read Bus misses

The right panel retains the last outer-lane attempt when returning to CENTER:

- Target type and ID; `TARGET: NONE / REASON: VEHICLE_ALREADY_USED` when only used IDs are available.
- Actual intersection time measured from the transfer request, player speed **before inheriting vehicle speed**, target speed, absolute difference, configured limit.
- `SPEED CHECK` and `POSITION CHECK`, followed by `SUCCESS / MISS`. While the event is pending, actual speed check is N/A; if no intersection occurs, speed check remains N/A rather than claiming an invented pass/failure.
- The predicted unmounted world range, Catch Zone endpoints and overlap interval remain available and are explicitly labeled as predictions, not the post-catch ridden trajectory.
- `VEHICLE USED: YES/NO`, current ID/type, remaining service, last-used ID and separate completion status, elapsed simulation time since the last actual exit, current speed/normal speed and deceleration (configured and active).

`POSITION PASS + SPEED FAIL` means a real intersection occurred with excessive difference. `POSITION FAIL + SPEED N/A` means no intersection occurred, so this attempt cannot establish whether the speed at a hypothetical contact would pass. `SUCCESS` is shown immediately on actual catch, even while lateral movement continues.

## Verification

- 12/12 CTest suites PASS. New `movement_feel_contract`: 240 checks covering A–G, contact0/.04/1/15/.08/.12/.17/.40, inclusive threshold, one-step versus 48-step agreement, failed-first-contact handling, configurable thresholds, and snapshots before inherited speed.
- A: exact 48 simulation steps at 120Hz, adjacent-only and discarded inputs.
- B/C: immediately consumed successful Bike cannot be selected again; failed Bus stays unused and remains available to later target selection.
- D: Express service exhaustion returns to CENTER without penalty and never restores its ID.
- E/F: 60→45→30→15→12 at 1/2/3/3.2s; 12→7 Miss and 5/s recovery.
- G: all four adjacent upgrades feasible; all six direct skips rejected by physics/limits, without special prohibitions.
- 16 default-config scripted full runs finish; 32 Express catches, 39 service ends, 16 actual catches of previously Prepared vehicles. 367,506 traffic/service assertions verify no reused ride IDs, constant trajectories, same-lane clearance, cross-lane reservations, per-lane no-repeat, admission-only statistics, off-screen births and varied sequences (2 distinct sequences).
- Historical analytic fixtures pin their original .25s/10/s inputs explicitly. Their assertions are retained for integration regression; V0.3 default behavior is covered by the new tests and updated end-to-end tests. Fixture mounts now use distinct IDs, consistent with real traffic.
- Native build and 120-frame graphical smoke run PASS, exit0 and screenshot saved. This supersedes the earlier monitor-init limitation for this run only.
- Web build PASS; browser start/retry, directional input, 0.40s label, 15/s label and Bus position-failure snapshot inspected in `evidence/v03-overlay-web.png`. Actual contact numbers were verified in deterministic tests; the browser screenshot is a no-intersection attempt. Neither smoke nor scripted runs replace owner playtesting.

Rank, deadline, finish distance, vehicle speed/length/zone/service values, minimum gap and TTC logic remain unchanged. The score-rules identifier changes to V0.3 so records from different mechanics are not compared; rank/time calculation is unchanged.

## Owner playtest

1. Does 0.40 s require anticipation without feeling sluggish? Check that mid-transfer taps never queue.
2. Is the approximately 66.7ms immediate Bike→Bus allowance too narrow? Compare actual contact time, player speed and difference, especially position PASS/speed FAIL.
3. Does 15/s noticeably reduce high-speed persistence? Test both successful upgrades and missed connections.
4. Is USED understandable, and can you confirm neither manual departure nor service end allows the same ID again?
5. Does Run→Bike→Bus→Taxi→Express remain readable in real traffic, with prepared opportunities visible early enough?

No further parameter tuning is performed before this feedback. **Pending playtest.** The full previous source state is preserved in ignored `.local/v02-before-v03`; this iteration remains uncommitted/unpushed pending user confirmation under the project Git workflow.


## First Playable acceptance — 2026-11-10

The owner confirmed actual playtesting and authorized a formal source commit, the `v0.1-playtest` milestone and browser delivery. Earlier Pending playtest notes above describe the state at the end of the tuning iteration; the current release status is accepted for external playtesting, not final balance. No gameplay changes were made for deployment. See [First Playable verification](FIRST_PLAYABLE.md).
