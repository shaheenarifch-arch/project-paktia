#include "havoc/strat/OpsLink.h"

namespace havoc::strat {

bool LinkInterdiction(const Strategic& strategic, const std::string& zoneId, ops::Theater& theater,
                      std::size_t opsRoute) {
  const std::size_t z = strategic.Map().FindZone(zoneId);
  if (z == kNone) return false;
  theater.SetInterdiction(opsRoute, strategic.Zones().at(z).threat);
  return true;
}

}  // namespace havoc::strat
