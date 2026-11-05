#pragma once
// One-way link strategic -> operational: the strategic threat on a sea zone
// becomes the interdiction of an operational supply route. The operational
// layer knows nothing about strat.
#include <cstddef>
#include <string>

#include "havoc/ops/Operational.h"
#include "havoc/strat/Strategic.h"

namespace havoc::strat {

// Sets ops route `opsRoute` interdiction to the threat on sea zone `zoneId`.
// Returns false (and changes nothing) when the zone id is unknown.
bool LinkInterdiction(const Strategic& strategic, const std::string& zoneId, ops::Theater& theater,
                      std::size_t opsRoute);

}  // namespace havoc::strat
