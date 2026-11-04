#include "havoc/World.h"

namespace havoc {

bool ComponentStore::Has(ComponentId c, EntityId e) const {
  auto it = data_.find(c);
  return it != data_.end() && it->second.count(e) != 0;
}

std::int64_t ComponentStore::Get(ComponentId c, EntityId e, std::int64_t fallback) const {
  auto it = data_.find(c);
  if (it == data_.end()) return fallback;
  auto jt = it->second.find(e);
  return jt == it->second.end() ? fallback : jt->second;
}

void ComponentStore::Remove(EntityId e) {
  for (auto& kv : data_) kv.second.erase(e);
}

const std::map<EntityId, std::int64_t>& ComponentStore::All(ComponentId c) const {
  static const std::map<EntityId, std::int64_t> empty;
  auto it = data_.find(c);
  return it == data_.end() ? empty : it->second;
}

World::World(std::uint64_t seed, ClockConfig cfg) : seed_(seed), clock_(cfg), rng_(seed) {
  clock_.OnStrategic([this](std::uint64_t) { RunLayer(Layer::Strategic); });
  clock_.OnOperational([this](std::uint64_t) { RunLayer(Layer::Operational); });
  clock_.OnTactical([this](std::uint64_t) { RunLayer(Layer::Tactical); });
}

void World::RegisterSystem(Layer layer, System s) {
  systems_[static_cast<int>(layer)].push_back(std::move(s));
}

void World::RunLayer(Layer l) {
  // Index loop: systems may not register systems mid-tick, but stay safe.
  auto& v = systems_[static_cast<int>(l)];
  for (std::size_t i = 0; i < v.size(); ++i) v[i](*this);
}

}  // namespace havoc
