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

### Game data in the sim

The sim does not parse JSON. `scripts/gen-sim-data.mjs` turns `data/units`, `data/formations` and `data/logistics` into `generated/HavocData.inc` (integer thousandths, no floats). After changing those data files run:

```
npm run sim:gen
```

`npm run sim:test` (and CI) fails if the generated file is out of date.
