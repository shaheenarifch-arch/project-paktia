#include "havoc/strat/Strategic.h"

#include <cstdint>

namespace havoc::strat {
namespace {

const Fixed64 kZero{};
const Fixed64 kOne = Fixed64::FromInt(1);

Fixed64 Min(Fixed64 a, Fixed64 b) { return a < b ? a : b; }
Fixed64 Max(Fixed64 a, Fixed64 b) { return a > b ? a : b; }
Fixed64 Clamp01(Fixed64 v) { return Min(kOne, Max(kZero, v)); }

struct Fnv {
  std::uint64_t h = 1469598103934665603ull;
  void Byte(std::uint8_t b) { h ^= b; h *= 1099511628211ull; }
  void U64(std::uint64_t v) { for (int i = 0; i < 8; ++i) Byte(static_cast<std::uint8_t>(v >> (8 * i))); }
  void I64(std::int64_t v) { U64(static_cast<std::uint64_t>(v)); }
  void F(Fixed64 v) { I64(v.raw); }
  void S(const std::string& s) { U64(s.size()); for (char c : s) Byte(static_cast<std::uint8_t>(c)); }
};

}  // namespace

Strategic::Strategic(const WorldMap& map, Tuning tuning) : map_(&map), tuning_(tuning) {
  for (const Province& p : map.Provinces()) {
    ProvinceState s;
    s.owner = p.owner;
    provinces_.push_back(s);
  }
  zones_.assign(map.Zones().size(), ZoneState{});
  routes_.assign(map.Routes().size(), RouteState{});
  for (const std::string& f : map.Factions()) {
    treasury_[f] = kZero;
    income_[f] = kZero;
  }
}

void Strategic::Attach(World& world) {
  world.RegisterSystem(Layer::Strategic, [this](World& w) { StepDay(&w); });
  world.AddCommandHandler([this](World& w, const Command& c) {
    if (c.kind != kCmdBlockade && c.kind != kCmdLiftBlockade) return;
    const auto& factions = map_->Factions();
    if (c.entity >= factions.size() || c.a < 0 || static_cast<std::uint64_t>(c.a) >= zones_.size()) return;
    const std::string& faction = factions.at(static_cast<std::size_t>(c.entity));
    const auto zone = static_cast<std::size_t>(c.a);
    if (c.kind == kCmdBlockade) {
      Blockade(faction, zone, &w);
    } else {
      LiftBlockade(faction, zone, &w);
    }
  });
}

bool Strategic::Blockade(const std::string& faction, std::size_t zone, World* world) {
  const std::size_t f = map_->FindFaction(faction);
  if (f == kNone || zone >= zones_.size()) return false;
  ZoneState& z = zones_.at(zone);
  if (!z.blockadedBy.empty()) return false;  // already held (by anyone): lift first
  z.blockadedBy = faction;
  if (world != nullptr) world->Emit(kEvBlockadeStarted, zone, static_cast<std::int64_t>(f));
  return true;
}

bool Strategic::LiftBlockade(const std::string& faction, std::size_t zone, World* world) {
  const std::size_t f = map_->FindFaction(faction);
  if (f == kNone || zone >= zones_.size()) return false;
  ZoneState& z = zones_.at(zone);
  if (z.blockadedBy != faction) return false;
  z.blockadedBy.clear();
  if (world != nullptr) world->Emit(kEvBlockadeLifted, zone, static_cast<std::int64_t>(f));
  return true;
}

bool Strategic::Capture(std::size_t province, const std::string& faction, World* world) {
  const std::size_t f = map_->FindFaction(faction);
  if (f == kNone || province >= provinces_.size()) return false;
  ProvinceState& p = provinces_.at(province);
  if (p.owner == faction) return false;
  const std::size_t old = map_->FindFaction(p.owner);
  p.owner = faction;
  p.control = tuning_.controlOnCapture;
  if (world != nullptr) {
    world->Emit(kEvProvinceCaptured, province, old == kNone ? -1 : static_cast<std::int64_t>(old),
                static_cast<std::int64_t>(f));
  }
  return true;
}

void Strategic::SetDamage(std::size_t province, Fixed64 damage) { provinces_.at(province).damage = Clamp01(damage); }

std::size_t Strategic::BlockingZone(std::size_t route, const std::string& faction) const {
  for (std::size_t z : map_->Routes().at(route).zoneLegs) {
    const std::string& by = zones_.at(z).blockadedBy;
    if (!by.empty() && map_->AreEnemies(by, faction)) return z;
  }
  return kNone;
}

bool Strategic::RouteOpenFor(std::size_t route, const std::string& faction) const {
  return BlockingZone(route, faction) == kNone;
}

void Strategic::UpdateRoutes(World* world) {
  for (std::size_t i = 0; i < routes_.size(); ++i) {
    RouteState& rs = routes_.at(i);
    const Route& route = map_->Routes().at(i);
    const std::size_t blocking = BlockingZone(i, route.owner);
    const bool open = blocking == kNone;

    // Interdiction: cut when closed, otherwise the worst residual threat on
    // its zones (a friendly blockade does not threaten the owner's own route).
    Fixed64 interdiction = kZero;
    if (!open) {
      interdiction = kOne;
    } else {
      for (std::size_t z : route.zoneLegs) {
        const std::string& by = zones_.at(z).blockadedBy;
        if (!by.empty()) continue;  // blockaded by a non-enemy
        interdiction = Max(interdiction, zones_.at(z).threat);
      }
    }
    rs.interdiction = interdiction;

    if (open != rs.open) {
      rs.open = open;
      if (world != nullptr) {
        if (open) world->Emit(kEvRouteOpened, i);
        else world->Emit(kEvRouteClosed, i, static_cast<std::int64_t>(blocking));
      }
    }
  }
}

Fixed64 Strategic::ComputeIncome(const std::string& faction) const {
  Fixed64 total = kZero;
  for (std::size_t p = 0; p < provinces_.size(); ++p) {
    const ProvinceState& ps = provinces_.at(p);
    const Fixed64 oil = map_->Provinces().at(p).oil;
    if (ps.owner != faction || oil <= kZero) continue;
    bool exports = false;
    for (std::size_t r = 0; r < routes_.size() && !exports; ++r) {
      exports = map_->Routes().at(r).reachesMarket && map_->RouteServes(r, p) && RouteOpenFor(r, faction);
    }
    if (exports) total += oil * (kOne - ps.damage) * ps.control;
  }
  return total;
}

Fixed64 Strategic::Income(const std::string& faction) const {
  const auto it = income_.find(faction);
  return it == income_.end() ? kZero : it->second;
}

Fixed64 Strategic::Treasury(const std::string& faction) const {
  const auto it = treasury_.find(faction);
  return it == treasury_.end() ? kZero : it->second;
}

void Strategic::StepDay(World* world) {
  // 1. Zone threat follows blockades.
  for (ZoneState& z : zones_) {
    if (!z.blockadedBy.empty()) z.threat = Min(kOne, z.threat + tuning_.threatRisePerDay);
    else z.threat = Max(kZero, z.threat - tuning_.threatFallPerDay);
  }
  // 2. Route state, interdiction and open/closed events.
  UpdateRoutes(world);
  // 3. Captured provinces slowly come under control.
  for (ProvinceState& p : provinces_) p.control = Min(kOne, p.control + tuning_.controlRecoverPerDay);
  // 4. Oil income and treasury, factions in sorted order.
  for (const std::string& f : map_->Factions()) {
    const Fixed64 income = ComputeIncome(f);
    income_[f] = income;
    treasury_[f] += income;
  }
}

std::uint64_t Strategic::Hash() const {
  Fnv f;
  f.U64(provinces_.size());
  for (const ProvinceState& p : provinces_) { f.S(p.owner); f.F(p.control); f.F(p.damage); }
  f.U64(zones_.size());
  for (const ZoneState& z : zones_) { f.S(z.blockadedBy); f.F(z.threat); }
  f.U64(routes_.size());
  for (const RouteState& r : routes_) { f.Byte(r.open ? 1 : 0); f.F(r.interdiction); }
  f.U64(treasury_.size());
  for (const auto& kv : treasury_) { f.S(kv.first); f.F(kv.second); }
  for (const auto& kv : income_) { f.S(kv.first); f.F(kv.second); }
  return f.h;
}

}  // namespace havoc::strat
