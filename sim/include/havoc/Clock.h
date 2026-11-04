#pragma once
// One master tick at 20 Hz (tactical). Operational (1 game-hour) and strategic
// (1 game-day) ticks are derived from it.
#include <cstdint>
#include <functional>

namespace havoc {

struct ClockConfig {
  static constexpr std::uint32_t kTacticalHz = 20;
  // Tactical ticks per game-hour. Default 1200 = 60 real seconds per game-hour.
  std::uint32_t ticksPerGameHour = 1200;
  std::uint32_t hoursPerGameDay = 24;
};

class Clock {
 public:
  using Callback = std::function<void(std::uint64_t tick)>;

  explicit Clock(ClockConfig cfg = {}) : cfg_(cfg) {}

  void OnTactical(Callback cb) { tactical_ = std::move(cb); }
  void OnOperational(Callback cb) { operational_ = std::move(cb); }
  void OnStrategic(Callback cb) { strategic_ = std::move(cb); }

  // Advance one master tick. Fires strategic, then operational, then tactical
  // callbacks. Operational fires when the new tick count is a multiple of
  // ticksPerGameHour; strategic when it is a multiple of a game-day.
  void Advance();

  std::uint64_t Tick() const { return tick_; }
  void SetTick(std::uint64_t t) { tick_ = t; }
  const ClockConfig& Config() const { return cfg_; }
  std::uint64_t TicksPerDay() const {
    return static_cast<std::uint64_t>(cfg_.ticksPerGameHour) * cfg_.hoursPerGameDay;
  }

 private:
  ClockConfig cfg_;
  std::uint64_t tick_ = 0;  // number of completed ticks
  Callback tactical_, operational_, strategic_;
};

}  // namespace havoc
