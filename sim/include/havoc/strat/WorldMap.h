#pragma once
// Strategic world map v1: provinces, sea zones (some of them chokepoints) and
// strategic routes, compiled in from data/world by scripts/gen-sim-data.mjs
// (see sim/generated/World.inc). Static data only; the changing state lives in
// strat::Strategic. No JSON parsing at runtime, no floating point.
#include <cstddef>
#include <cstdint>
#include <string>
#include <utility>
#include <vector>

#include "havoc/Fixed.h"

namespace havoc::strat {

constexpr std::size_t kNone = static_cast<std::size_t>(-1);

struct Province {
  std::string id, name, owner, terrain, population;  // owner is the starting owner
  std::int32_t infrastructure = 0;                   // 0..10
  Fixed64 oil;                                       // game units per day
  bool isPort = false;
  bool hasAirbase = false;
  std::vector<std::size_t> adjacentProvinces;  // indices into WorldMap::Provinces()
  std::vector<std::size_t> adjacentZones;      // indices into WorldMap::Zones()
};

struct SeaZone {
  std::string id, name;
  bool chokepoint = false;
  bool worldMarket = false;  // shipping that reaches this zone reaches a world market
  std::vector<std::size_t> adjacentProvinces;
  std::vector<std::size_t> adjacentZones;
};

// One step of a route: a province or a sea zone.
struct Leg {
  bool zone = false;
  std::size_t index = 0;  // into Provinces() or Zones()
};

enum class RouteMode : int { Sea = 0, Land = 1, Mixed = 2 };

struct Route {
  std::string id, name, owner;
  RouteMode mode = RouteMode::Sea;
  std::vector<Leg> legs;                 // consecutive legs are adjacent
  std::vector<std::size_t> exporters;    // provinces whose oil uses this route (besides province legs)
  std::vector<std::size_t> zoneLegs;     // derived: indices of the sea zones among the legs
  bool reachesMarket = false;            // derived: some leg is a worldMarket zone
};

class WorldMap {
 public:
  const std::vector<Province>& Provinces() const { return provinces_; }
  const std::vector<SeaZone>& Zones() const { return zones_; }
  const std::vector<Route>& Routes() const { return routes_; }
  const std::vector<std::string>& Factions() const { return factions_; }  // sorted by id

  // kNone when the id is unknown.
  std::size_t FindProvince(const std::string& id) const;
  std::size_t FindZone(const std::string& id) const;
  std::size_t FindRoute(const std::string& id) const;
  std::size_t FindFaction(const std::string& id) const;

  bool AreEnemies(const std::string& a, const std::string& b) const;

  // Does `route` use province `p` for its oil (as an exporter or a province leg)?
  bool RouteServes(std::size_t route, std::size_t p) const;

 private:
  friend WorldMap BuildWorldMap();
  std::vector<Province> provinces_;
  std::vector<SeaZone> zones_;
  std::vector<Route> routes_;
  std::vector<std::string> factions_;
  std::vector<std::pair<std::string, std::string>> enemies_;
};

// Built once from the generated data.
const WorldMap& BuiltinWorldMap();

}  // namespace havoc::strat
