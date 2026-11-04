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
