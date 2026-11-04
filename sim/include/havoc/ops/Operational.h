#pragma once
// Operational layer v1: one theater of brigades, depots and supply routes,
// stepped once per game-hour on the World's operational tick.
//
// Models (design doc section 3, operational layer and realistic effects):
//   - supply by class (fuel, ammunition, spares, food/water, medical)
//   - consumption by posture, from the unit data in data/units
//   - resupply from depots along capacity-limited, interdictable routes
//   - fatigue, morale, equipment wear, refit, and readiness
//
// Everything is Fixed64 and iterates in index order, so a theater is
// deterministic and hashes identically on every platform.
#include <array>
#include <cstdint>
#include <string>
#include <vector>

#include "havoc/Fixed.h"
#include "havoc/World.h"
#include "havoc/ops/Data.h"

namespace havoc::ops {

enum class Supply : int { Fuel = 0, Ammo = 1, Spares = 2, Food = 3, Medical = 4 };
constexpr int kSupplyClasses = 5;
using Stock = std::array<Fixed64, kSupplyClasses>;

inline Fixed64& At(Stock& s, Supply c) { return s.at(static_cast<std::size_t>(c)); }
inline Fixed64 At(const Stock& s, Supply c) { return s.at(static_cast<std::size_t>(c)); }

enum class Posture : int { Rest = 0, Defend = 1, Move = 2, Attack = 3, Refit = 4 };

// Event kinds this layer writes to the World event log (range 100-199).
enum EventKind : std::uint32_t {
  kEvShortage = 100,        // entity = brigade index, a = Supply class
  kEvPostureForced = 101,   // a = old posture, b = new posture
  kEvAttackRejected = 102,  // a = readiness raw
  kEvRefitRejected = 103,   // brigade not at a depot
  kEvPostureChanged = 104,  // a = old posture, b = new posture
};

// Command kinds this layer accepts (range 100-199).
enum CommandKind : std::uint32_t {
  kCmdSetPosture = 100,  // entity = brigade index, a = Posture
  kCmdMoveTo = 101,      // entity = brigade index, a = node index (arrives next hour, v1)
};

// Tunables. Rates are per game-day unless noted. Kept in one place so balance
// can move to data/ later without touching the model.
struct Tuning {
  // Fraction of daily fuel burn used by posture.
  std::array<Fixed64, 5> fuelUse = {Fixed64::FromRatio(1, 10), Fixed64::FromRatio(3, 10), Fixed64::FromInt(1),
                                    Fixed64::FromRatio(8, 10), Fixed64::FromRatio(1, 20)};
  // Fraction of the basic ammunition load fired per day by posture.
  std::array<Fixed64, 5> ammoUse = {Fixed64{}, Fixed64::FromRatio(3, 10), Fixed64{}, Fixed64::FromInt(1), Fixed64{}};
  // Multiplier on daily maintenance hours by posture.
  std::array<Fixed64, 5> wearUse = {Fixed64::FromRatio(1, 2), Fixed64::FromInt(1), Fixed64::FromRatio(3, 2),
                                    Fixed64::FromInt(2), Fixed64{}};
  // Fatigue change per game-hour by posture (negative = recovery).
  std::array<Fixed64, 5> fatiguePerHour = {Fixed64::FromRatio(-2, 100), Fixed64::FromRatio(5, 1000),
                                           Fixed64::FromRatio(1, 100), Fixed64::FromRatio(2, 100),
                                           Fixed64::FromRatio(-3, 100)};
  Fixed64 sparesPerRepairHour = Fixed64::FromRatio(1, 10);  // crates
  Fixed64 daysOfSparesCarried = Fixed64::FromInt(3);
  Fixed64 daysOfFoodCarried = Fixed64::FromInt(3);
  Fixed64 equipmentWearPerHour = Fixed64::FromRatio(1, 1000);        // Move/Attack with spares
  Fixed64 equipmentWearNoSparesPerHour = Fixed64::FromRatio(4, 1000);
  Fixed64 refitEquipmentPerHour = Fixed64::FromRatio(1, 100);
  Fixed64 refitPersonnelPerHour = Fixed64::FromRatio(5, 1000);
  Fixed64 moraleBaseline = Fixed64::FromRatio(7, 10);
  Fixed64 moraleDriftPerHour = Fixed64::FromRatio(5, 1000);
  Fixed64 moraleRestBonusPerHour = Fixed64::FromRatio(1, 100);
  Fixed64 moraleAmmoShortPerHour = Fixed64::FromRatio(2, 100);
  Fixed64 moraleFoodShortPerHour = Fixed64::FromRatio(1, 100);
  Fixed64 moraleExhaustedPerHour = Fixed64::FromRatio(1, 100);
  Fixed64 exhaustedFatigue = Fixed64::FromRatio(8, 10);
  Fixed64 personnelLossNoMedicalPerHour = Fixed64::FromRatio(2, 1000);
  Fixed64 attackReadinessFloor = Fixed64::FromRatio(6, 10);  // design doc: <60% not fit for offensive tasks
};

struct Node {
  std::string id;
  bool depot = false;
  Stock stock{};
  Fixed64 throughputPerDay;  // tons-equivalent a depot can issue per day to brigades on the node
};

struct Route {
  std::size_t from = 0, to = 0;  // node indices
  Fixed64 capacityPerDay;        // supply units per day across all classes
  Fixed64 interdiction;          // 0 (open) .. 1 (cut)
};

struct Brigade {
  std::string id;
  std::string faction;
  std::string formation;
  std::size_t node = 0;
  Posture posture = Posture::Defend;

  // Derived once from the formation's units.
  std::int32_t crew = 0;
  Stock capacity{};            // basic load the brigade can carry
  Fixed64 fuelBurnPerDay;      // at full movement
  Fixed64 maintenancePerDay;   // repair-hours per day of operation

  // State, all 0..1 except stock.
  Stock stock{};
  Fixed64 personnel = Fixed64::FromInt(1);
  Fixed64 equipment = Fixed64::FromInt(1);
  Fixed64 fatigue;
  Fixed64 morale = Fixed64::FromRatio(7, 10);
  Fixed64 readiness = Fixed64::FromInt(1);
  std::array<bool, kSupplyClasses> shortage{};  // set during the last hour
};

class Theater {
 public:
  explicit Theater(const DataSet& data = BuiltinData(), Tuning tuning = {}) : data_(&data), tuning_(tuning) {}

  std::size_t AddNode(std::string id, bool depot, Stock stock = {}, Fixed64 throughputPerDay = {});
  std::size_t AddRoute(std::size_t from, std::size_t to, Fixed64 capacityPerDay, Fixed64 interdiction = {});
  // Builds a brigade from a formation in data/formations, fully supplied.
  // Returns SIZE_MAX if the formation id is unknown.
  std::size_t AddBrigade(std::string id, const std::string& formation, std::size_t node);

  // Wires this theater into a World: an operational system that calls
  // StepHour, and a command handler for kCmdSetPosture / kCmdMoveTo.
  void Attach(World& world);

  // One game-hour. World is optional (events are only logged when given).
  void StepHour(World* world = nullptr);

  // Applies a posture change with the design-doc rules (attack needs
  // readiness >= 60%, refit needs a depot). Returns false when rejected.
  bool SetPosture(std::size_t brigade, Posture p, World* world = nullptr);
  void MoveTo(std::size_t brigade, std::size_t node, World* world = nullptr);
  // Combat losses from the tactical layer, as fractions of current strength.
  void ApplyLosses(std::size_t brigade, Fixed64 personnelLoss, Fixed64 equipmentLoss);

  void SetInterdiction(std::size_t route, Fixed64 v);

  const std::vector<Node>& Nodes() const { return nodes_; }
  const std::vector<Route>& Routes() const { return routes_; }
  const std::vector<Brigade>& Brigades() const { return brigades_; }
  std::vector<Node>& MutableNodes() { return nodes_; }
  std::vector<Brigade>& MutableBrigades() { return brigades_; }
  const Tuning& GetTuning() const { return tuning_; }

  // Stable FNV-1a hash of the whole theater state.
  std::uint64_t Hash() const;

 private:
  void Consume(Brigade& b, std::size_t index, World* world);
  void Resupply();
  void UpdateCondition(Brigade& b);
  Fixed64 ComputeReadiness(const Brigade& b) const;
  void Issue(Node& from, Brigade& b, Fixed64& budget);

  const DataSet* data_;
  Tuning tuning_;
  std::vector<Node> nodes_;
  std::vector<Route> routes_;
  std::vector<Brigade> brigades_;
};

}  // namespace havoc::ops
