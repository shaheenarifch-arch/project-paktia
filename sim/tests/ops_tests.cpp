// Operational layer v1 tests (run in both test executables).
#include <limits>

#include "TestHarness.h"
#include "havoc/Fixed.h"
#include "havoc/World.h"
#include "havoc/StateHash.h"
#include "havoc/ops/Data.h"
#include "havoc/ops/Operational.h"

using namespace havoc;

namespace opstest {

using namespace havoc::ops;

// |a - b| within a few raw units (fixed-point rounding over many steps).
bool Near(Fixed64 a, Fixed64 b, std::int64_t tol = 4096) {
  const std::int64_t d = a.raw - b.raw;
  return d <= tol && d >= -tol;
}

// One depot (port) linked to a forward node by a single route.
struct Setup {
  Theater t;
  std::size_t port = 0, front = 0, route = 0, meu = 0;
  explicit Setup(Fixed64 routeCapacity = Fixed64::FromInt(400)) {
    Stock depotStock{};
    for (Fixed64& v : depotStock) v = Fixed64::FromInt(100000);
    port = t.AddNode("kharg_port", true, depotStock, Fixed64::FromInt(2000));
    front = t.AddNode("kharg_north_beach", false);
    route = t.AddRoute(port, front, routeCapacity);
    meu = t.AddBrigade("meu_1", "coalition_marine_expeditionary_unit", front);
  }
};

}  // namespace opstest

TEST(ops_data_loaded) {
  const auto& d = havoc::ops::BuiltinData();
  CHECK(d.units.size() >= 45);
  CHECK(d.FindUnit("coalition_main_battle_tank") != nullptr);
  CHECK(d.FindUnit("no_such_unit") == nullptr);
  const auto* meu = d.FindFormation("coalition_marine_expeditionary_unit");
  CHECK(meu != nullptr);
  if (meu != nullptr) CHECK(!meu->entries.empty());
  CHECK(d.foodPerCrewPerDay == Fixed64::FromRatio(700, 1000));
  for (std::size_t i = 1; i < d.units.size(); ++i) CHECK(d.units.at(i - 1).id < d.units.at(i).id);
}

TEST(ops_brigade_built_from_formation) {
  opstest::Setup s;
  const auto& b = s.t.Brigades().at(s.meu);
  CHECK(s.meu == 0);
  CHECK(b.faction == "coalition");
  CHECK(b.crew > 100);
  CHECK(havoc::ops::At(b.capacity, havoc::ops::Supply::Fuel) > Fixed64{});
  CHECK(b.stock == b.capacity);
  CHECK(b.readiness > Fixed64::FromRatio(6, 10));
  CHECK(s.t.AddBrigade("x", "no_such_formation", s.front) == std::numeric_limits<std::size_t>::max());
}

TEST(ops_fuel_burn_matches_posture) {
  // No route, so nothing is resupplied: a day of movement burns one day of fuel.
  havoc::ops::Theater t;
  const auto n = t.AddNode("desert", false);
  const auto b = t.AddBrigade("armor_1", "coalition_armored_brigade", n);
  const Fixed64 start = havoc::ops::At(t.Brigades().at(b).stock, havoc::ops::Supply::Fuel);
  CHECK(t.SetPosture(b, havoc::ops::Posture::Move));
  for (int h = 0; h < 24; ++h) t.StepHour();
  const auto& br = t.Brigades().at(b);
  const Fixed64 used = start - havoc::ops::At(br.stock, havoc::ops::Supply::Fuel);
  CHECK(opstest::Near(used, br.fuelBurnPerDay));
  CHECK(br.fatigue > Fixed64::FromRatio(2, 10));
  CHECK(br.equipment < Fixed64::FromInt(1));
}

TEST(ops_route_capacity_and_interdiction) {
  using havoc::ops::Supply;
  // Drain the brigade, then compare one hour of resupply on an open and a half-cut route.
  auto deliveredInOneHour = [](Fixed64 interdiction) {
    opstest::Setup s(Fixed64::FromInt(240));  // 10 per hour
    s.t.SetInterdiction(s.route, interdiction);
    auto& b = s.t.MutableBrigades().at(s.meu);
    for (Fixed64& v : b.stock) v = Fixed64{};
    s.t.StepHour();
    Fixed64 total{};
    for (const Fixed64& v : s.t.Brigades().at(s.meu).stock) total += v;
    return total;
  };
  const Fixed64 open = deliveredInOneHour(Fixed64{});
  const Fixed64 half = deliveredInOneHour(Fixed64::FromRatio(1, 2));
  const Fixed64 cut = deliveredInOneHour(Fixed64::FromInt(1));
  CHECK(opstest::Near(open, Fixed64::FromInt(10)));
  CHECK(opstest::Near(half, Fixed64::FromInt(5)));
  CHECK(cut == Fixed64{});
}

TEST(ops_ammo_is_issued_first) {
  using havoc::ops::Supply;
  opstest::Setup s(Fixed64::FromInt(24));  // 1 per hour, less than the ammo gap
  auto& b = s.t.MutableBrigades().at(s.meu);
  for (Fixed64& v : b.stock) v = Fixed64{};
  s.t.StepHour();
  const auto& after = s.t.Brigades().at(s.meu);
  CHECK(opstest::Near(havoc::ops::At(after.stock, Supply::Ammo), Fixed64::FromInt(1)));
  CHECK(havoc::ops::At(after.stock, Supply::Fuel) == Fixed64{});
}

TEST(ops_no_fuel_forces_defend) {
  havoc::World w(7);
  havoc::ops::Theater t;
  t.Attach(w);
  const auto n = t.AddNode("desert", false);
  const auto b = t.AddBrigade("armor_1", "coalition_armored_brigade", n);
  CHECK(t.SetPosture(b, havoc::ops::Posture::Move, &w));
  havoc::ops::At(t.MutableBrigades().at(b).stock, havoc::ops::Supply::Fuel) = Fixed64{};
  t.StepHour(&w);
  CHECK(t.Brigades().at(b).posture == havoc::ops::Posture::Defend);
  bool forced = false, shortage = false;
  for (const auto& e : w.Log().Events()) {
    if (e.kind == havoc::ops::kEvPostureForced) forced = true;
    if (e.kind == havoc::ops::kEvShortage && e.a == static_cast<int>(havoc::ops::Supply::Fuel)) shortage = true;
  }
  CHECK(forced);
  CHECK(shortage);
}

TEST(ops_attack_needs_readiness) {
  opstest::Setup s;
  auto& b = s.t.MutableBrigades().at(s.meu);
  CHECK(s.t.SetPosture(s.meu, havoc::ops::Posture::Attack));
  CHECK(s.t.SetPosture(s.meu, havoc::ops::Posture::Defend));
  s.t.ApplyLosses(s.meu, Fixed64::FromRatio(1, 2), Fixed64::FromRatio(1, 2));
  CHECK(b.readiness < Fixed64::FromRatio(6, 10));
  CHECK(!s.t.SetPosture(s.meu, havoc::ops::Posture::Attack));
  CHECK(b.posture == havoc::ops::Posture::Defend);
}

TEST(ops_rest_and_refit_recover) {
  opstest::Setup s;
  auto& b = s.t.MutableBrigades().at(s.meu);
  b.fatigue = Fixed64::FromRatio(9, 10);
  CHECK(!s.t.SetPosture(s.meu, havoc::ops::Posture::Refit));  // not on a depot
  CHECK(s.t.SetPosture(s.meu, havoc::ops::Posture::Rest));
  for (int h = 0; h < 12; ++h) s.t.StepHour();
  CHECK(b.fatigue < Fixed64::FromRatio(9, 10));

  b.node = s.port;
  b.equipment = Fixed64::FromRatio(5, 10);
  CHECK(s.t.SetPosture(s.meu, havoc::ops::Posture::Refit));
  for (int h = 0; h < 24; ++h) s.t.StepHour();
  CHECK(b.equipment > Fixed64::FromRatio(7, 10));
}

TEST(ops_deterministic_with_commands) {
  auto run = [](std::int64_t posture) {
    havoc::ClockConfig cfg;
    cfg.ticksPerGameHour = 10;
    havoc::World w(42, cfg);
    opstest::Setup s;
    s.t.Attach(w);
    for (std::uint64_t i = 0; i < 10 * 24 * 10; ++i) {  // ten game-days
      if (i == 50) w.ApplyCommand(havoc::Command{i, havoc::ops::kCmdSetPosture, s.meu, posture, 0});
      if (i == 700) w.ApplyCommand(havoc::Command{i, havoc::ops::kCmdMoveTo, s.meu, static_cast<std::int64_t>(s.port), 0});
      w.Step();
    }
    return s.t.Hash() ^ (havoc::HashWorld(w) * 31);
  };
  const auto a = run(static_cast<std::int64_t>(havoc::ops::Posture::Attack));
  const auto b = run(static_cast<std::int64_t>(havoc::ops::Posture::Attack));
  const auto c = run(static_cast<std::int64_t>(havoc::ops::Posture::Rest));
  CHECK(a == b);
  CHECK(a != c);
}
