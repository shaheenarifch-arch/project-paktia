#include "havoc/ops/Operational.h"

#include <cstdint>
#include <limits>

namespace havoc::ops {
namespace {

const Fixed64 kZero{};
const Fixed64 kOne = Fixed64::FromInt(1);
const Fixed64 kHour = Fixed64::FromRatio(1, 24);  // one game-hour as a fraction of a day

Fixed64 Min(Fixed64 a, Fixed64 b) { return a < b ? a : b; }
Fixed64 Max(Fixed64 a, Fixed64 b) { return a > b ? a : b; }
Fixed64 Clamp01(Fixed64 v) { return Min(kOne, Max(kZero, v)); }
std::size_t Idx(Posture p) { return static_cast<std::size_t>(p); }

// Order in which a depot fills a brigade's needs.
constexpr Supply kIssueOrder[] = {Supply::Ammo, Supply::Fuel, Supply::Food, Supply::Medical, Supply::Spares};

struct Fnv {
  std::uint64_t h = 1469598103934665603ull;
  void Byte(std::uint8_t b) { h ^= b; h *= 1099511628211ull; }
  void U64(std::uint64_t v) { for (int i = 0; i < 8; ++i) Byte(static_cast<std::uint8_t>(v >> (8 * i))); }
  void I64(std::int64_t v) { U64(static_cast<std::uint64_t>(v)); }
  void F(Fixed64 v) { I64(v.raw); }
  void S(const std::string& s) { U64(s.size()); for (char c : s) Byte(static_cast<std::uint8_t>(c)); }
  void St(const Stock& s) { for (const Fixed64& v : s) F(v); }
};

}  // namespace

std::size_t Theater::AddNode(std::string id, bool depot, Stock stock, Fixed64 throughputPerDay) {
  nodes_.push_back(Node{std::move(id), depot, stock, throughputPerDay});
  return nodes_.size() - 1;
}

std::size_t Theater::AddRoute(std::size_t from, std::size_t to, Fixed64 capacityPerDay, Fixed64 interdiction) {
  routes_.push_back(Route{from, to, capacityPerDay, Clamp01(interdiction)});
  return routes_.size() - 1;
}

std::size_t Theater::AddBrigade(std::string id, const std::string& formation, std::size_t node) {
  const FormationDef* f = data_->FindFormation(formation);
  if (f == nullptr) return std::numeric_limits<std::size_t>::max();

  Brigade b;
  b.id = std::move(id);
  b.faction = f->faction;
  b.formation = f->id;
  b.node = node;
  for (const FormationEntry& e : f->entries) {
    const UnitDef& u = data_->units.at(e.unit);
    const Fixed64 n = Fixed64::FromInt(e.count);
    b.crew += u.crew * e.count;
    At(b.capacity, Supply::Fuel) += u.fuelCapacity * n;
    At(b.capacity, Supply::Ammo) += u.ammoCrates * n;
    b.fuelBurnPerDay += u.fuelBurnPerDay * n;
    b.maintenancePerDay += u.maintenancePerDay * n;
  }
  const Fixed64 crew = Fixed64::FromInt(b.crew);
  At(b.capacity, Supply::Spares) = b.maintenancePerDay * tuning_.sparesPerRepairHour * tuning_.daysOfSparesCarried;
  At(b.capacity, Supply::Food) = crew * data_->foodPerCrewPerDay * tuning_.daysOfFoodCarried;
  At(b.capacity, Supply::Medical) = crew * data_->medicalPerCrewPerDay * tuning_.daysOfFoodCarried;
  b.stock = b.capacity;
  b.morale = tuning_.moraleBaseline;
  b.readiness = ComputeReadiness(b);
  brigades_.push_back(std::move(b));
  return brigades_.size() - 1;
}

void Theater::Attach(World& world) {
  world.RegisterSystem(Layer::Operational, [this](World& w) { StepHour(&w); });
  world.SetCommandHandler([this](World& w, const Command& c) {
    if (c.entity >= brigades_.size()) return;
    const auto b = static_cast<std::size_t>(c.entity);
    if (c.kind == kCmdSetPosture && c.a >= 0 && c.a <= static_cast<std::int64_t>(Posture::Refit)) {
      SetPosture(b, static_cast<Posture>(c.a), &w);
    } else if (c.kind == kCmdMoveTo && c.a >= 0 && static_cast<std::uint64_t>(c.a) < nodes_.size()) {
      MoveTo(b, static_cast<std::size_t>(c.a), &w);
    }
  });
}

void Theater::StepHour(World* world) {
  for (std::size_t i = 0; i < brigades_.size(); ++i) Consume(brigades_.at(i), i, world);
  Resupply();
  for (Brigade& b : brigades_) UpdateCondition(b);
}

void Theater::Consume(Brigade& b, std::size_t index, World* world) {
  const std::size_t p = Idx(b.posture);
  Stock need{};
  At(need, Supply::Fuel) = b.fuelBurnPerDay * tuning_.fuelUse.at(p) * kHour;
  At(need, Supply::Ammo) = At(b.capacity, Supply::Ammo) * tuning_.ammoUse.at(p) * kHour;
  At(need, Supply::Spares) = b.maintenancePerDay * tuning_.wearUse.at(p) * tuning_.sparesPerRepairHour * kHour;
  const Fixed64 crew = Fixed64::FromInt(b.crew) * b.personnel;
  At(need, Supply::Food) = crew * data_->foodPerCrewPerDay * kHour;
  At(need, Supply::Medical) = crew * data_->medicalPerCrewPerDay * kHour;

  for (int c = 0; c < kSupplyClasses; ++c) {
    const auto ci = static_cast<std::size_t>(c);
    Fixed64& have = b.stock.at(ci);
    const Fixed64 want = need.at(ci);
    const bool wasShort = b.shortage.at(ci);
    const bool isShort = want > have;
    have = isShort ? kZero : have - want;
    b.shortage.at(ci) = isShort;
    if (isShort && !wasShort && world != nullptr) world->Emit(kEvShortage, index, c);
  }

  // No fuel: the brigade cannot manoeuvre and digs in.
  if (b.shortage.at(static_cast<std::size_t>(Supply::Fuel)) &&
      (b.posture == Posture::Move || b.posture == Posture::Attack)) {
    const Posture old = b.posture;
    b.posture = Posture::Defend;
    if (world != nullptr) world->Emit(kEvPostureForced, index, static_cast<int>(old), static_cast<int>(b.posture));
  }
}

void Theater::Issue(Node& from, Brigade& b, Fixed64& budget) {
  for (Supply c : kIssueOrder) {
    if (budget <= kZero) return;
    Fixed64& have = At(b.stock, c);
    Fixed64& depot = At(from.stock, c);
    const Fixed64 amount = Min(Min(At(b.capacity, c) - have, depot), budget);
    if (amount <= kZero) continue;
    have += amount;
    depot -= amount;
    budget -= amount;
  }
}

void Theater::Resupply() {
  // Brigades on a depot node draw directly, up to the depot's throughput.
  for (std::size_t n = 0; n < nodes_.size(); ++n) {
    Node& node = nodes_.at(n);
    if (!node.depot) continue;
    Fixed64 budget = node.throughputPerDay * kHour;
    for (Brigade& b : brigades_) {
      if (b.node == n) Issue(node, b, budget);
    }
  }
  // Routes from a depot carry supply to brigades on the far node.
  for (const Route& r : routes_) {
    Node& from = nodes_.at(r.from);
    if (!from.depot) continue;
    Fixed64 budget = r.capacityPerDay * kHour * (kOne - r.interdiction);
    for (Brigade& b : brigades_) {
      if (b.node == r.to) Issue(from, b, budget);
    }
  }
}

void Theater::UpdateCondition(Brigade& b) {
  const std::size_t p = Idx(b.posture);
  const bool ammoShort = b.shortage.at(static_cast<std::size_t>(Supply::Ammo));
  const bool foodShort = b.shortage.at(static_cast<std::size_t>(Supply::Food));
  const bool sparesShort = b.shortage.at(static_cast<std::size_t>(Supply::Spares));
  const bool medicalShort = b.shortage.at(static_cast<std::size_t>(Supply::Medical));

  // Fatigue.
  Fixed64 fatigue = b.fatigue + tuning_.fatiguePerHour.at(p);
  if (foodShort) fatigue += Fixed64::FromRatio(1, 100);
  b.fatigue = Clamp01(fatigue);

  // Equipment wear and refit.
  if (b.posture == Posture::Move || b.posture == Posture::Attack) {
    b.equipment = Clamp01(b.equipment - (sparesShort ? tuning_.equipmentWearNoSparesPerHour : tuning_.equipmentWearPerHour));
  } else if (b.posture == Posture::Refit && nodes_.at(b.node).depot) {
    if (!sparesShort) b.equipment = Clamp01(b.equipment + tuning_.refitEquipmentPerHour);
    b.personnel = Clamp01(b.personnel + tuning_.refitPersonnelPerHour);
  }
  if (medicalShort) b.personnel = Clamp01(b.personnel - tuning_.personnelLossNoMedicalPerHour);

  // Morale drifts toward its baseline, then events push it.
  Fixed64 m = b.morale;
  if (m < tuning_.moraleBaseline) m = Min(tuning_.moraleBaseline, m + tuning_.moraleDriftPerHour);
  else if (m > tuning_.moraleBaseline) m = Max(tuning_.moraleBaseline, m - tuning_.moraleDriftPerHour);
  if (b.posture == Posture::Rest || b.posture == Posture::Refit) m += tuning_.moraleRestBonusPerHour;
  if (ammoShort && (b.posture == Posture::Defend || b.posture == Posture::Attack)) m -= tuning_.moraleAmmoShortPerHour;
  if (foodShort) m -= tuning_.moraleFoodShortPerHour;
  if (b.fatigue >= tuning_.exhaustedFatigue) m -= tuning_.moraleExhaustedPerHour;
  b.morale = Clamp01(m);

  b.readiness = ComputeReadiness(b);
}

Fixed64 Theater::ComputeReadiness(const Brigade& b) const {
  Fixed64 supply = kOne;
  for (Supply c : {Supply::Fuel, Supply::Ammo, Supply::Food}) {
    const Fixed64 cap = At(b.capacity, c);
    if (cap > kZero) supply = Min(supply, At(b.stock, c) / cap);
  }
  const Fixed64 rested = kOne - b.fatigue * Fixed64::FromRatio(1, 2);
  const Fixed64 base = Min(Min(b.personnel, b.equipment), Min(supply, rested));
  return Clamp01(base * (Fixed64::FromRatio(1, 2) + b.morale * Fixed64::FromRatio(1, 2)));
}

bool Theater::SetPosture(std::size_t index, Posture p, World* world) {
  Brigade& b = brigades_.at(index);
  if (b.posture == p) return true;
  if (p == Posture::Attack && b.readiness < tuning_.attackReadinessFloor) {
    if (world != nullptr) world->Emit(kEvAttackRejected, index, b.readiness.raw);
    return false;
  }
  if (p == Posture::Refit && !nodes_.at(b.node).depot) {
    if (world != nullptr) world->Emit(kEvRefitRejected, index);
    return false;
  }
  const Posture old = b.posture;
  b.posture = p;
  if (world != nullptr) world->Emit(kEvPostureChanged, index, static_cast<int>(old), static_cast<int>(p));
  return true;
}

void Theater::MoveTo(std::size_t index, std::size_t node, World* world) {
  Brigade& b = brigades_.at(index);
  if (node >= nodes_.size() || node == b.node) return;
  // v1: the move completes within the hour; movement time per route comes with
  // the theater map in a later step. Fuel shortage still blocks movement.
  if (At(b.stock, Supply::Fuel) <= kZero && At(b.capacity, Supply::Fuel) > kZero) return;
  b.node = node;
  SetPosture(index, Posture::Move, world);
}

void Theater::ApplyLosses(std::size_t index, Fixed64 personnelLoss, Fixed64 equipmentLoss) {
  Brigade& b = brigades_.at(index);
  b.personnel = Clamp01(b.personnel - b.personnel * Clamp01(personnelLoss));
  b.equipment = Clamp01(b.equipment - b.equipment * Clamp01(equipmentLoss));
  b.morale = Clamp01(b.morale - Clamp01(personnelLoss) * Fixed64::FromRatio(1, 2));
  b.readiness = ComputeReadiness(b);
}

void Theater::SetInterdiction(std::size_t route, Fixed64 v) { routes_.at(route).interdiction = Clamp01(v); }

std::uint64_t Theater::Hash() const {
  Fnv f;
  f.U64(nodes_.size());
  for (const Node& n : nodes_) { f.S(n.id); f.Byte(n.depot ? 1 : 0); f.St(n.stock); f.F(n.throughputPerDay); }
  f.U64(routes_.size());
  for (const Route& r : routes_) { f.U64(r.from); f.U64(r.to); f.F(r.capacityPerDay); f.F(r.interdiction); }
  f.U64(brigades_.size());
  for (const Brigade& b : brigades_) {
    f.S(b.id); f.S(b.formation); f.U64(b.node); f.I64(static_cast<int>(b.posture));
    f.St(b.stock); f.F(b.personnel); f.F(b.equipment); f.F(b.fatigue); f.F(b.morale); f.F(b.readiness);
    for (bool s : b.shortage) f.Byte(s ? 1 : 0);
  }
  return f.h;
}

}  // namespace havoc::ops
