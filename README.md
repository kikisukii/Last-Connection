# LAST CONNECTION

[**PLAY IN BROWSER**](https://kikisukii.github.io/Last-Connection/)

A minimalist two-button time-attack gameplay prototype about predicting relative velocity and timing transfers between moving vehicles.

**First Playable / Playtest Build** — accepted for external playtesting by the project owner. Prototype balance is still subject to feedback.

## Play

[**PLAY IN BROWSER**](https://kikisukii.github.io/Last-Connection/) — no installation required. The deployed build passed a fresh-browser full-run check, including boarding, finish/rank and best-time persistence.

Current accepted tuning: Bike / Bus / Taxi / Express ride services last **4 / 3 / 2 / 1.5 game seconds**. See [Ride service duration](docs/RIDE_SERVICE_DURATION.md).

For local play, see [Build and run](docs/BUILD.md).

## Controls

**← / →** moves one adjacent lane. Click the canvas to focus. The first key press starts the run. Transfers take 0.40 seconds; inputs during a transfer are discarded. After finishing, press either key or click RUN AGAIN to retry.

## Goal

Reach the airport as quickly as possible. Catch a vehicle's bright zone during transfer with an acceptable relative velocity. Each vehicle connection can be used once.

## Rank

**S / A / B / C / D / F**. The deadline is 90 game seconds; late arrivals may still finish and receive F. Best time is saved in this browser on this device. Scores use simulation time, which can slow with low frame rates or background browser throttling.

## Technical

C++20 · CMake · raylib 5.5 · Emscripten 4.0.23 · WebAssembly · GitHub Pages workflow

The deployed version needs only a modern desktop browser and keyboard. Source builds need the toolchain documented below. Native binaries, generated WASM, SDKs and caches are not committed.

## Development

- [Design](docs/DESIGN.md)
- [Iteration history](docs/ITERATIONS.md)
- [Technical notes](docs/TECHNICAL_NOTES.md)
- [Build and deployment](docs/BUILD.md)
- [Current gameplay rules](docs/GAMEPLAY_V0_3.md)
- [First Playable verification](docs/FIRST_PLAYABLE.md)

The project owner owns the concept, mechanics, playtesting and tuning decisions. Codex assists with implementation, tests, debugging and delivery. Automated checks are distinct from human playtesting.
