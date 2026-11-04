#pragma once
#include <cstdint>

#include "havoc/World.h"

namespace havoc {

// FNV-1a 64-bit over a canonical serialization of the whole world state
// (tick, seed, RNG state, entity counter, components, event log).
std::uint64_t HashWorld(const World& w);

}  // namespace havoc
