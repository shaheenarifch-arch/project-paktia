#include "havoc/Replay.h"

#include "havoc/StateHash.h"

namespace havoc {

std::uint64_t Run(const Recording& rec, const SetupFn& setup) {
  World w(rec.seed, rec.clock);
  setup(w);
  std::size_t next = 0;
  for (std::uint64_t t = 1; t <= rec.ticks; ++t) {
    // Commands stamped with tick t apply before the step that runs tick t.
    while (next < rec.commands.size() && rec.commands[next].tick <= t) {
      w.ApplyCommand(rec.commands[next]);
      ++next;
    }
    w.Step();
  }
  return HashWorld(w);
}

void Record(Recording& rec, const SetupFn& setup) { rec.finalHash = Run(rec, setup); }

bool Verify(const Recording& rec, const SetupFn& setup) { return Run(rec, setup) == rec.finalHash; }

}  // namespace havoc
