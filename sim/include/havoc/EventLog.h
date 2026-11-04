#pragma once
#include <cstdint>
#include <vector>

namespace havoc {

struct Event {
  std::uint64_t tick = 0;
  std::uint32_t kind = 0;
  std::uint64_t entity = 0;
  std::int64_t a = 0;
  std::int64_t b = 0;
};

// Append-only; events are stored in the order appended (tick is non-decreasing).
class EventLog {
 public:
  void Append(const Event& e) { events_.push_back(e); }
  const std::vector<Event>& Events() const { return events_; }
  std::size_t Size() const { return events_.size(); }
  std::vector<Event> ForTick(std::uint64_t tick) const {
    std::vector<Event> out;
    for (const Event& e : events_)
      if (e.tick == tick) out.push_back(e);
    return out;
  }

 private:
  std::vector<Event> events_;
};

}  // namespace havoc
