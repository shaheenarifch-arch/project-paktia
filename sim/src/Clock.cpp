#include "havoc/Clock.h"

namespace havoc {

void Clock::Advance() {
  ++tick_;
  const std::uint64_t perHour = cfg_.ticksPerGameHour;
  if (perHour != 0) {
    if (tick_ % TicksPerDay() == 0 && strategic_) strategic_(tick_);
    if (tick_ % perHour == 0 && operational_) operational_(tick_);
  }
  if (tactical_) tactical_(tick_);
}

}  // namespace havoc
