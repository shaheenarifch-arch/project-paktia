#pragma once
#include <cstdint>
#include <functional>
#include <map>
#include <vector>

#include "havoc/Clock.h"
#include "havoc/EventLog.h"
#include "havoc/Random.h"

namespace havoc {

using EntityId = std::uint64_t;
using ComponentId = std::uint32_t;

enum class Layer : int { Strategic = 0, Operational = 1, Tactical = 2 };

// Component store: ordered maps only (std::map) so iteration order is
// deterministic. Values are int64 (use Fixed64::raw for fixed-point data).
class ComponentStore {
 public:
  void Set(ComponentId c, EntityId e, std::int64_t v) { data_[c][e] = v; }
  bool Has(ComponentId c, EntityId e) const;
  std::int64_t Get(ComponentId c, EntityId e, std::int64_t fallback = 0) const;
  void Remove(EntityId e);  // remove all components of an entity
  const std::map<ComponentId, std::map<EntityId, std::int64_t>>& Data() const { return data_; }
  const std::map<EntityId, std::int64_t>& All(ComponentId c) const;

 private:
  std::map<ComponentId, std::map<EntityId, std::int64_t>> data_;
};

struct Command {
  std::uint64_t tick = 0;  // applied before the step that executes this tick
  std::uint32_t kind = 0;
  EntityId entity = 0;
  std::int64_t a = 0;
  std::int64_t b = 0;
};

class World {
 public:
  using System = std::function<void(World&)>;
  using CommandHandler = std::function<void(World&, const Command&)>;

  explicit World(std::uint64_t seed, ClockConfig cfg = {});
  World(const World&) = delete;
  World& operator=(const World&) = delete;

  EntityId CreateEntity() { return nextEntity_++; }
  void DestroyEntity(EntityId e) { components_.Remove(e); }

  // Systems run in registration order within their layer. Layers run
  // strategic -> operational -> tactical on ticks where they fire.
  void RegisterSystem(Layer layer, System s);
  void SetCommandHandler(CommandHandler h) { handler_ = std::move(h); }
  void ApplyCommand(const Command& c) { if (handler_) handler_(*this, c); }

  // Advance one master tick.
  void Step() { clock_.Advance(); }

  // Number of completed ticks. During a system call this is the tick being run
  // (1-based).
  std::uint64_t Tick() const { return clock_.Tick(); }
  std::uint64_t Seed() const { return seed_; }
  EntityId NextEntityId() const { return nextEntity_; }

  Pcg32& Rng() { return rng_; }
  const Pcg32& Rng() const { return rng_; }
  ComponentStore& Components() { return components_; }
  const ComponentStore& Components() const { return components_; }
  EventLog& Log() { return log_; }
  const EventLog& Log() const { return log_; }
  const Clock& GetClock() const { return clock_; }

  void Emit(std::uint32_t kind, EntityId e, std::int64_t a = 0, std::int64_t b = 0) {
    log_.Append(Event{clock_.Tick(), kind, e, a, b});
  }

 private:
  void RunLayer(Layer l);

  std::uint64_t seed_;
  Clock clock_;
  Pcg32 rng_;
  ComponentStore components_;
  EventLog log_;
  EntityId nextEntity_ = 1;
  std::vector<System> systems_[3];
  CommandHandler handler_;
};

}  // namespace havoc
