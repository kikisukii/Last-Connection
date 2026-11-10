# Start screen and onboarding — 2026-11-11

Scope: UI/onboarding only, requested for the browser playtest build. The existing 22 gameplay/config headers and sources are unchanged from `71edd5d`; see [protected source audit](evidence/onboarding-protected-sources.txt). No speed, thresholds, zones, transfer duration, decay/recovery, penalty, service duration, traffic/TTC, Rank, Deadline or finish logic changed.

## Presentation

The first screen is opaque and contains no debug overlay. Native renders three cards with the existing raylib colors/font. Web renders semantic HTML above the canvas with the same copy supplied from `include/onboarding.hpp`; no duplicate translated rules or independently maintained gameplay values are embedded. Desktop cards cover controls/lanes, prediction/boarding, and misses/temporary rides. A common footer explains the deadline and ranks and contains START. Narrow windows stack the cards; short landscape windows put the footer and button side by side. Rank thresholds and deadline seconds are not hardcoded in the tutorial.

Copy, in reading order:

```text
LAST CONNECTION
A TWO-BUTTON TIME ATTACK
HOW TO PLAY
REACH THE AIRPORT AS FAST AS YOU CAN.

← / →  CHANGE TO AN ADJACENT LANE
CENTER
Run at normal speed.
LEFT / RIGHT
Ride passing vehicles to gain speed.

PREDICT THE INTERSECTION
Don't wait until you're next to a vehicle.
Move early and aim for where it will be.
BOARDING
If your timing and speed are right, you'll board automatically.

MISS?
Keep running. You'll lose speed, but you can recover.
EVERY RIDE IS TEMPORARY.
Each vehicle can only be used once.
Look for your next connection.

REACH THE AIRPORT BEFORE THE DEADLINE.
Finish faster to improve your rank.
S  A  B  C  D  F
S = fastest    F = missed flight
START
```

Native displays outlined arrow keys using its available glyphs. Web also shows a small LEFT / CENTER / RIGHT map where space permits. This remains a keyboard game; responsive onboarding does not add touchscreen driving controls.

## Menu behavior

- Fresh load: Start. Simulation remains paused until START or an arrow press.
- The start frame consumes the entire input and resets only the frame accumulator. It cannot also call `requestTransfer` or advance the simulation with menu time.
- Results: compact time/rank/best, PLAY AGAIN and optional HOW TO PLAY. Immediate replay never forces the tutorial.
- HOW TO PLAY: opens the tutorial over the existing result; BACK returns without restarting or changing the result. PLAY AGAIN starts a new run.
- Web buttons retain normal keyboard accessibility. Enter/Space activate a focused menu button; arrow-to-start is consumed before the gameplay keyboard handler. No extra boarding input was introduced.
- Debug defaults off and F1 toggles it during play. Menu screens stay opaque and clean even if debug was enabled previously.
- Existing record persistence, restart seed selection and fixed-step simulation remain in place.

## Verification

Native build, graphical startup/exit smoke, and 14/14 CTest suites passed. The added UI state contract covers initial state, menu-only input, opt-in debug, optional tutorial/back and direct replay. Native startup screenshot was visually inspected.

The isolated Chromium test checks the complete copy and START visibility at 1400×820, 1024×600, 800×600, 640×480, 640×360, 390×844, 360×640 and 320×568. It rejects clipped text/container bounds and document scrolling. Mouse START and both arrow start routes leave the player in CENTER; F1 enables and hides debug. Web build uses the existing Emscripten 4.0.23 workflow. CMake now tracks shell HTML as a link dependency so CSS-only edits regenerate the output.

Local full-browser run PASS: 98.74 game seconds, rank F, no page errors; result/help/back, all help viewport bounds, direct replay and persisted best passed. [Result JSON](evidence/onboarding-browser.json).

The same browser test also runs a real Bike catch, return to CENTER, full finish, result/help/back, direct replay and best persistence, both on the packaged site and on the actual Pages URL in CI. It does not inject game state, accelerate time or modify gameplay configuration. Deployment success is determined by the repository Actions result, not by the presence of a push.

Evidence: [Native screen](evidence/onboarding-native.png), [Web desktop](evidence/onboarding-web.png), [Web narrow](evidence/onboarding-narrow.png), [CTest](evidence/onboarding-ctest.txt).

Automated UI checks and visual inspection are distinct from user acceptance of how easily newcomers learn. **Pending playtest** for first-time-player comprehension; no balance changes or claims.
