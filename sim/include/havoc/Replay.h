#pragma once
#include <cstdint>
#include <functional>
#include <vector>

#include "havoc/World.h"

namespace havoc {

// A recording is the seed, the tick count, and the ordered player commands.
struct Recording {
  std::uint64_t seed = 0;
  std::uint64_t ticks = 0;
  ClockConfig clock;
  std::vector<Command> commands;  // must be ordered by tick (stable for ties)
  std::uint64_t finalHash = 0;    // filled in by Record()
};

// Builds the initial world: entities, systems, command handler. Must itself be
// deterministic.
using SetupFn = std::function<void(World&)>;

// Run seed + commands for `ticks` ticks, return the final state hash.
std::uint64_t Run(const Recording& rec, const SetupFn& setup);

// Run and store the resulting hash in rec.finalHash.
void Record(Recording& rec, const SetupFn& setup);

// Re-run the recording; true if the hash matches rec.finalHash.
bool Verify(const Recording& rec, const SetupFn& setup);

}  // namespace havoc
