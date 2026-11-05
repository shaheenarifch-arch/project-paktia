#pragma once
// Strategic layer v1, stepped once per game-day on the World's strategic tick
// (Strategic::Attach, like ops::Theater::Attach).
//
// Models (design doc, strategic layer): chokepoint blockades, strategic routes,
// oil income and economic warfare.
//   - A zone can be blockaded by one faction. A route is open for a faction
//     only if none of its sea zones is blockaded by an enemy of that faction.
//   - A blockade raises the zone's threat toward 1 each day; it falls back
//     after the blockade is lifted. A route's interdiction is the highest
//     threat among its zones (1 when closed).
//   - Daily oil income per faction = sum over its provinces of
//     oil x (1 - infrastructure damage) x control, counting only provinces
//     whose oil has an open route to a world market.
//   - Treasury grows by income each day.
//
// Everything is Fixed64 and iterates in index order, so a Strategic hashes
// identically on every platform.
#include <cstdint>
#include <map>
#include <string>
#include <vector>

#include "havoc/Fixed.h"
#include "havoc/World.h"
#include "havoc/strat/WorldMap.h"

namespace havoc::strat {

// Event kinds this layer writes to the World event log (range 200-299).
enum EventKind : std::uint32_t {
  kEvBlockadeStarted = 200,  // entity = zone index, a = faction index
  kEvBlockadeLifted = 201,   // entity = zone index, a = faction index
  kEvRouteClosed = 202,      // entity = route index, a = zone index that closed it
  kEvRouteOpened = 203,      // entity = route index
  kEvProvinceCaptured = 204  // entity = province index, a = old owner index, b = new owner index
};

// Command kinds this layer accepts (range 200-299). Faction indices are
// positions in WorldMap::Factions().
enum CommandKind : std::uint32_t {
  kCmdBlockade = 200,      // entity = faction index, a = zone index
  kCmdLiftBlockade = 201,  // entity = faction index, a = zone index (only the blockading faction can lift)
};

struct Tuning {
  Fixed64 threatRisePerDay = Fixed64::FromRatio(1, 4);   // zone threat while blockaded
  Fixed64 threatFallPerDay = Fixed64::FromRatio(1, 8);   // after the blockade is lifted
  Fixed64 controlOnCapture = Fixed64::FromRatio(1, 4);   // a newly captured province starts here
  Fixed64 controlRecoverPerDay = Fixed64::FromRatio(1, 20);
};

struct ProvinceState {
  std::string owner;
  Fixed64 control = Fixed64::FromInt(1);  // 0..1
  Fixed64 damage;                         // infrastructure damage 0..1
};

struct ZoneState {
  std::string blockadedBy;  // faction id, empty when free
  Fixed64 threat;           // 0..1
};

struct RouteState {
  bool open = true;         // open for its owner
  Fixed64 interdiction;     // 0 (open) .. 1 (cut)
};

class Strategic {
 public:
  explicit Strategic(const WorldMap& map = BuiltinWorldMap(), Tuning tuning = {});

  // Wires this layer into a World: a strategic system that calls StepDay, and
  // a command handler for kCmdBlockade / kCmdLiftBlockade.
  void Attach(World& world);

  // One game-day. World is optional (events are only logged when given).
  void StepDay(World* world = nullptr);

  // Returns false when rejected (unknown ids, zone already blockaded by
  // another faction, or nothing to lift).
  bool Blockade(const std::string& faction, std::size_t zone, World* world = nullptr);
  bool LiftBlockade(const std::string& faction, std::size_t zone, World* world = nullptr);
  // Hands a province to `faction`; control restarts low and recovers.
  bool Capture(std::size_t province, const std::string& faction, World* world = nullptr);
  void SetDamage(std::size_t province, Fixed64 damage);

  // Is the route open for `faction` right now (no zone blockaded by an enemy)?
  bool RouteOpenFor(std::size_t route, const std::string& faction) const;
  // First zone on the route blockaded by an enemy of `faction`, or kNone.
  std::size_t BlockingZone(std::size_t route, const std::string& faction) const;

  // Oil income per day for a faction given the current state (does not change it).
  Fixed64 ComputeIncome(const std::string& faction) const;
  Fixed64 Income(const std::string& faction) const;    // as of the last StepDay
  Fixed64 Treasury(const std::string& faction) const;

  const WorldMap& Map() const { return *map_; }
  const std::vector<ProvinceState>& Provinces() const { return provinces_; }
  const std::vector<ZoneState>& Zones() const { return zones_; }
  const std::vector<RouteState>& Routes() const { return routes_; }
  const Tuning& GetTuning() const { return tuning_; }

  // Stable FNV-1a hash of all strategic state.
  std::uint64_t Hash() const;

 private:
  void UpdateRoutes(World* world);

  const WorldMap* map_;
  Tuning tuning_;
  std::vector<ProvinceState> provinces_;
  std::vector<ZoneState> zones_;
  std::vector<RouteState> routes_;
  std::map<std::string, Fixed64> treasury_;
  std::map<std::string, Fixed64> income_;
};

}  // namespace havoc::strat
