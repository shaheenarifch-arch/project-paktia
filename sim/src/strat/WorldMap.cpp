#include "havoc/strat/WorldMap.h"

#include <algorithm>
#include <utility>

namespace havoc::strat {
namespace {

Fixed64 Milli(std::int64_t v) { return Fixed64::FromRatio(v, 1000); }

template <typename T>
std::size_t FindById(const std::vector<T>& v, const std::string& id) {
  for (std::size_t i = 0; i < v.size(); ++i) {
    if (v.at(i).id == id) return i;
  }
  return kNone;
}

}  // namespace

WorldMap BuildWorldMap() {
  WorldMap m;
  std::vector<std::pair<std::string, std::string>> adjacency;
  struct PendingLeg { std::string route, leg; };
  std::vector<PendingLeg> legs, exporters;

#define HAVOC_FACTION(id) m.factions_.push_back(id);
#define HAVOC_ENEMY(a, b) m.enemies_.emplace_back(a, b);
#define HAVOC_PROVINCE(id_, name_, owner_, terrain_, infra_, oilMilli_, pop_, port_, air_) \
  {                                                                                          \
    Province p;                                                                              \
    p.id = id_;                                                                              \
    p.name = name_;                                                                          \
    p.owner = owner_;                                                                        \
    p.terrain = terrain_;                                                                    \
    p.infrastructure = (infra_);                                                             \
    p.oil = Milli(oilMilli_);                                                                \
    p.population = pop_;                                                                     \
    p.isPort = (port_);                                                                      \
    p.hasAirbase = (air_);                                                                   \
    m.provinces_.push_back(p);                                                               \
  }
#define HAVOC_ZONE(id_, name_, choke_, market_) \
  {                                             \
    SeaZone z;                                  \
    z.id = id_;                                 \
    z.name = name_;                             \
    z.chokepoint = (choke_);                    \
    z.worldMarket = (market_);                  \
    m.zones_.push_back(z);                      \
  }
#define HAVOC_ADJACENT(a, b) adjacency.emplace_back(a, b);
#define HAVOC_ROUTE(id_, name_, mode_, owner_)                                                  \
  {                                                                                             \
    Route r;                                                                                    \
    r.id = id_;                                                                                 \
    r.name = name_;                                                                             \
    r.owner = owner_;                                                                           \
    const std::string mode(mode_);                                                              \
    r.mode = mode == "land" ? RouteMode::Land : (mode == "mixed" ? RouteMode::Mixed : RouteMode::Sea); \
    m.routes_.push_back(r);                                                                     \
  }
#define HAVOC_ROUTE_LEG(route, leg) legs.push_back(PendingLeg{route, leg});
#define HAVOC_ROUTE_EXPORTER(route, province) exporters.push_back(PendingLeg{route, province});
#include "../../generated/World.inc"
#undef HAVOC_ROUTE_EXPORTER
#undef HAVOC_ROUTE_LEG
#undef HAVOC_ROUTE
#undef HAVOC_ADJACENT
#undef HAVOC_ZONE
#undef HAVOC_PROVINCE
#undef HAVOC_ENEMY
#undef HAVOC_FACTION

  // Resolve adjacency ids (provinces and zones share one id space).
  for (const auto& edge : adjacency) {
    const std::size_t fp = FindById(m.provinces_, edge.first), fz = FindById(m.zones_, edge.first);
    const std::size_t tp = FindById(m.provinces_, edge.second), tz = FindById(m.zones_, edge.second);
    std::vector<std::size_t>* out = nullptr;
    std::size_t target = kNone;
    if (fp != kNone) {
      out = tp != kNone ? &m.provinces_.at(fp).adjacentProvinces : &m.provinces_.at(fp).adjacentZones;
      target = tp != kNone ? tp : tz;
    } else if (fz != kNone) {
      out = tp != kNone ? &m.zones_.at(fz).adjacentProvinces : &m.zones_.at(fz).adjacentZones;
      target = tp != kNone ? tp : tz;
    }
    if (out != nullptr && target != kNone) out->push_back(target);
  }

  for (const PendingLeg& l : legs) {
    const std::size_t r = FindById(m.routes_, l.route);
    if (r == kNone) continue;
    Route& route = m.routes_.at(r);
    const std::size_t p = FindById(m.provinces_, l.leg), z = FindById(m.zones_, l.leg);
    if (p != kNone) {
      route.legs.push_back(Leg{false, p});
    } else if (z != kNone) {
      route.legs.push_back(Leg{true, z});
      route.zoneLegs.push_back(z);
      if (m.zones_.at(z).worldMarket) route.reachesMarket = true;
    }
  }
  for (const PendingLeg& e : exporters) {
    const std::size_t r = FindById(m.routes_, e.route), p = FindById(m.provinces_, e.leg);
    if (r != kNone && p != kNone) m.routes_.at(r).exporters.push_back(p);
  }
  return m;
}

std::size_t WorldMap::FindProvince(const std::string& id) const { return FindById(provinces_, id); }
std::size_t WorldMap::FindZone(const std::string& id) const { return FindById(zones_, id); }
std::size_t WorldMap::FindRoute(const std::string& id) const { return FindById(routes_, id); }

std::size_t WorldMap::FindFaction(const std::string& id) const {
  for (std::size_t i = 0; i < factions_.size(); ++i) {
    if (factions_.at(i) == id) return i;
  }
  return kNone;
}

bool WorldMap::AreEnemies(const std::string& a, const std::string& b) const {
  for (const auto& e : enemies_) {
    if ((e.first == a && e.second == b) || (e.first == b && e.second == a)) return true;
  }
  return false;
}

bool WorldMap::RouteServes(std::size_t route, std::size_t p) const {
  const Route& r = routes_.at(route);
  if (std::find(r.exporters.begin(), r.exporters.end(), p) != r.exporters.end()) return true;
  for (const Leg& l : r.legs) {
    if (!l.zone && l.index == p) return true;
  }
  return false;
}

const WorldMap& BuiltinWorldMap() {
  static const WorldMap map = BuildWorldMap();
  return map;
}

}  // namespace havoc::strat
