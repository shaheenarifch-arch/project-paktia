# havoc_sim: deterministic simulation core

Engine-independent C++20 simulation core (design doc section 4). Unreal, the battle servers and the world server link it later. It has no dependencies beyond the standard library.

Contents (`include/havoc/`): `Fixed.h` (32.32 `Fixed64`), `Random.h` (PCG32), `Clock.h` (20 Hz master tick; game-hour and game-day derived, configurable via `ticksPerGameHour`), `World.h` (entity IDs, component store, systems in fixed order per layer), `EventLog.h`, `StateHash.h` (FNV-1a over the whole state), `Replay.h` (seed + ordered commands, re-run and compare hashes).

## Build and test

Needs CMake 3.16+ and g++ or clang.

```sh
cmake -S sim -B build/sim && cmake --build build/sim && ctest --test-dir build/sim
```

Or `npm run sim:test` from the repo root. Warnings are errors (`-Wall -Wextra -Werror`).

## Determinism rules

- No floats or doubles in sim logic. Use `Fixed64`.
- Never iterate `std::unordered_*` containers in sim logic. Use ordered containers (`std::map`) or vectors.
- No wall clock or `std::random_device`. Time is the tick counter; randomness is the seeded `Pcg32` owned by the `World`.
- Systems run in registration order within a layer; layers run strategic, operational, tactical.
- Player input enters only as `Command`s stamped with a tick, so it can be recorded and replayed.

## Help

- CMake documentation: https://cmake.org/cmake/help/latest/
- CMake tutorial: https://cmake.org/cmake/help/latest/guide/tutorial/index.html
- CTest: https://cmake.org/cmake/help/latest/manual/ctest.1.html

## Compilers

GCC and Clang use the built-in `__int128` for fixed-point multiply and divide. MSVC (Unreal on Windows) has no `__int128`, so `Fixed.h` falls back to a portable 128-bit path that gives bit-identical results. The `havoc_sim_tests_portable` test runs every test on that path (via `HAVOC_FORCE_PORTABLE_I128`), and `fixed_math_golden` checks both paths against one checksum.

## Operational layer (v1)

`include/havoc/ops/` holds the first gameplay layer on the core: one theater of brigades, depots and supply routes, stepped once per game-hour on the World's operational tick (`Theater::Attach`).

| Piece | What it does |
| --- | --- |
| `Data.h` | Unit and formation definitions compiled in from `data/` (see below). |
| `Operational.h` | `Theater` with nodes, routes and brigades; supply by class (fuel, ammunition, spares, food/water, medical); consumption by posture; resupply from depots along capacity-limited, interdictable routes; fatigue, morale, equipment wear, refit and readiness. |
| `Tuning` | Every balance number in one struct, so it can move to `data/` later. |

Rules from the design doc: a brigade below 60% readiness can't be ordered to attack; refit only happens on a depot; a brigade out of fuel stops moving and digs in. Commands (`kCmdSetPosture`, `kCmdMoveTo`) and events (`kEvShortage`, `kEvPostureForced`, ...) go through the World, so replays and hashes cover them.

## Strategic layer (v1)

`include/havoc/strat/` holds the world map and the daily strategic step, linked to the World's strategic tick (`Strategic::Attach`). Coupling is one-way: strat knows ops (`OpsLink.h`), ops knows nothing about strat.

| Piece | What it does |
| --- | --- |
| `WorldMap.h` | Static map from `data/world/gardun-gulf.json`: 16 provinces (owner, terrain, infrastructure 0-10, oil in game units/day, population band, `isPort`, `hasAirbase`), 12 sea zones (Hormuz, Bab el-Mandeb and Suez flagged `chokepoint`; Arabian Sea and the eastern Mediterranean are `worldMarket` zones), 5 strategic routes, and which factions are enemies. |
| `Strategic.h` | State per province (owner, control 0..1, infrastructure damage), per zone (`blockadedBy`, `threat` 0..1) and per route (open, interdiction), plus faction treasuries. `StepDay(World*)` runs once per game-day. |
| `OpsLink.h` | `LinkInterdiction` sets an `ops::Theater` route's interdiction from the threat on a sea zone. |

Daily rules, in order:

1. Zone threat rises 1/4 per day while blockaded and falls 1/8 per day after the blockade is lifted.
2. A route is open for a faction only if none of its sea zones is blockaded by an enemy of that faction. Its interdiction is 1 when closed, otherwise the highest residual threat on its zones. Opening or closing emits an event.
3. A captured province starts at low control and recovers 1/20 per day.
4. Oil income per faction = sum of its provinces' oil x (1 - infrastructure damage) x control, counting only provinces whose oil has an open route to a world market (province legs and `exporters` of a route that reaches a `worldMarket` zone). The treasury grows by the income.

Events (200-299): `kEvBlockadeStarted`, `kEvBlockadeLifted`, `kEvRouteClosed`, `kEvRouteOpened`, `kEvProvinceCaptured`. Commands: `kCmdBlockade` and `kCmdLiftBlockade` (`entity` = faction index, `a` = zone index; faction indices are positions in `WorldMap::Factions()`, sorted by id). Several layers can handle commands because `World::AddCommandHandler` appends handlers (`SetCommandHandler` still replaces them all). `Strategic::Hash()` covers all strategic state.

### Layout: adding a module

- Library sources: any `src/**/*.cpp` is picked up (CMake `GLOB_RECURSE ... CONFIGURE_DEPENDS`).
- Tests: `tests/TestHarness.h` has `TEST` and `CHECK`; each module has its own `tests/<module>_tests.cpp` (all `tests/*.cpp` link into both test executables). `main.cpp` has `main()` and the core tests only.
- Generated data: `scripts/gen-sim-data.mjs` runs every `scripts/sim-data/*.mjs` in sorted order (files starting with `_` are helpers). A section exports `output` and a default function returning the lines of `generated/<output>.inc`.

| Section | Reads | Writes |
| --- | --- | --- |
| `scripts/sim-data/ops.mjs` | `data/units`, `data/formations`, `data/logistics` | `generated/HavocData.inc` |
| `scripts/sim-data/world.mjs` | `data/world`, `data/factions` | `generated/World.inc` |

### Game data in the sim

The sim does not parse JSON. `scripts/gen-sim-data.mjs` turns the data folders above into `generated/*.inc` (integer thousandths, no floats). After changing those data files run:

```
npm run sim:gen
```

`npm run sim:test` (and CI) fails if any generated file is out of date.
