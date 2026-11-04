// Strategic layer v1 tests: world map, chokepoints, blockade, oil income.
// Run in both test executables (the second uses the portable 128-bit path).
#include <algorithm>

#include "TestHarness.h"
#include "havoc/Fixed.h"
#include "havoc/StateHash.h"
#include "havoc/World.h"
#include "havoc/ops/Operational.h"
#include "havoc/strat/OpsLink.h"
#include "havoc/strat/Strategic.h"
#include "havoc/strat/WorldMap.h"

namespace {

using havoc::Fixed64;
namespace strat = havoc::strat;

constexpr std::size_t kNoIndex = strat::kNone;

bool Contains(const std::vector<std::size_t>& v, std::size_t x) { return std::find(v.begin(), v.end(), x) != v.end(); }

// Gulf oil production of the coalition's exporters with every route open, and
// the same for Iran, from data/world/gulf-theatre.json (game units per day).
Fixed64 Units(std::int32_t n) { return Fixed64::FromInt(n); }

std::size_t Zone(const strat::WorldMap& m, const char* id) {
  const std::size_t z = m.FindZone(id);
  CHECK(z != kNoIndex);
  return z;
}
std::size_t Route(const strat::WorldMap& m, const char* id) {
  const std::size_t r = m.FindRoute(id);
  CHECK(r != kNoIndex);
  return r;
}

void StepDays(strat::Strategic& s, int days) {
  for (int i = 0; i < days; ++i) s.StepDay();
}

bool HasEvent(const havoc::World& w, std::uint32_t kind) {
  for (const havoc::Event& e : w.Log().Events()) {
    if (e.kind == kind) return true;
  }
  return false;
}

}  // namespace

TEST(strat_map_loads_and_adjacency_symmetric) {
  const strat::WorldMap& m = strat::BuiltinWorldMap();
  CHECK(m.Provinces().size() == 16);
  CHECK(m.Zones().size() == 12);
  CHECK(m.Routes().size() >= 4);
  CHECK(m.Factions().size() == 4);

  for (const char* id : {"hormuz", "bab_el_mandeb", "suez"}) {
    const std::size_t z = m.FindZone(id);
    CHECK(z != kNoIndex);
    if (z != kNoIndex) CHECK(m.Zones().at(z).chokepoint);
  }
  CHECK(m.FindProvince("iran_kharg") != kNoIndex);
  CHECK(m.FindProvince("no_such_province") == kNoIndex);
  CHECK(m.AreEnemies("coalition", "iran") && m.AreEnemies("iran", "coalition"));
  CHECK(!m.AreEnemies("iran", "houthi"));

  for (std::size_t i = 0; i < m.Provinces().size(); ++i) {
    const strat::Province& p = m.Provinces().at(i);
    CHECK(!p.adjacentProvinces.empty() || !p.adjacentZones.empty());
    for (std::size_t j : p.adjacentProvinces) CHECK(Contains(m.Provinces().at(j).adjacentProvinces, i));
    for (std::size_t j : p.adjacentZones) CHECK(Contains(m.Zones().at(j).adjacentProvinces, i));
  }
  for (std::size_t i = 0; i < m.Zones().size(); ++i) {
    const strat::SeaZone& z = m.Zones().at(i);
    for (std::size_t j : z.adjacentZones) CHECK(Contains(m.Zones().at(j).adjacentZones, i));
    for (std::size_t j : z.adjacentProvinces) CHECK(Contains(m.Provinces().at(j).adjacentZones, i));
  }
  // Consecutive route legs are neighbours.
  for (const strat::Route& r : m.Routes()) {
    CHECK(r.legs.size() >= 2);
    for (std::size_t k = 1; k < r.legs.size(); ++k) {
      const strat::Leg a = r.legs.at(k - 1), b = r.legs.at(k);
      const std::vector<std::size_t>& adj =
          a.zone ? (b.zone ? m.Zones().at(a.index).adjacentZones : m.Zones().at(a.index).adjacentProvinces)
                 : (b.zone ? m.Provinces().at(a.index).adjacentZones : m.Provinces().at(a.index).adjacentProvinces);
      CHECK(Contains(adj, b.index));
    }
  }
}

TEST(strat_hormuz_closure_cuts_gulf_income_and_lift_restores) {
  strat::Strategic s;
  const strat::WorldMap& m = s.Map();
  const std::size_t hormuz = Zone(m, "hormuz");
  const std::size_t gulfOil = Route(m, "gulf_oil_east");
  const std::size_t iranOil = Route(m, "iran_oil_east");

  s.StepDay();
  // Everything open: coalition exports 30+40+90+5+25+40+2 (Gulf) + 8 (Suez); Iran 35+10+45+5.
  CHECK(s.Income("coalition") == Units(240));
  CHECK(s.Income("iran") == Units(95));
  CHECK(s.Treasury("coalition") == Units(240));
  CHECK(s.Routes().at(gulfOil).open);

  // Iran blockades Hormuz: the coalition's Gulf route closes, Iran's own stays open.
  CHECK(s.Blockade("iran", hormuz));
  CHECK(!s.Blockade("coalition", hormuz));  // already held
  s.StepDay();
  CHECK(!s.Routes().at(gulfOil).open);
  CHECK(s.Routes().at(gulfOil).interdiction == Units(1));
  CHECK(s.Routes().at(iranOil).open);
  CHECK(s.Income("coalition") == Units(8));  // only the Suez exporter is left
  CHECK(s.Income("iran") == Units(95));
  CHECK(s.Treasury("coalition") == Units(248));

  // Lifting restores the route and the income; interdiction decays back to zero.
  CHECK(!s.LiftBlockade("coalition", hormuz));  // only the blockader can lift
  CHECK(s.LiftBlockade("iran", hormuz));
  s.StepDay();
  CHECK(s.Routes().at(gulfOil).open);
  CHECK(s.Income("coalition") == Units(240));
  CHECK(s.Routes().at(gulfOil).interdiction > Fixed64{});
  StepDays(s, 10);
  CHECK(s.Routes().at(gulfOil).interdiction == Fixed64{});
  CHECK(s.Zones().at(hormuz).threat == Fixed64{});
}

TEST(strat_coalition_blockade_closes_iran_route_only) {
  strat::Strategic s;
  const strat::WorldMap& m = s.Map();
  CHECK(s.Blockade("coalition", Zone(m, "hormuz")));
  s.StepDay();
  CHECK(!s.Routes().at(Route(m, "iran_oil_east")).open);
  CHECK(s.Routes().at(Route(m, "gulf_oil_east")).open);
  CHECK(s.Income("iran") == Fixed64{});
  CHECK(s.Income("coalition") == Units(240));
}

TEST(strat_bab_el_mandeb_cuts_houthi_oil) {
  strat::Strategic s;
  const strat::WorldMap& m = s.Map();
  s.StepDay();
  CHECK(s.Income("houthi") == Units(6));  // the highlands export via Hudaydah and Bab el-Mandeb
  CHECK(s.Blockade("coalition", Zone(m, "bab_el_mandeb")));
  s.StepDay();
  CHECK(s.Income("houthi") == Fixed64{});
  CHECK(!s.Routes().at(Route(m, "yemen_oil_export")).open);
  CHECK(s.Income("iran") == Units(95));
}

TEST(strat_kharg_damage_lowers_iran_income) {
  strat::Strategic s;
  const std::size_t kharg = s.Map().FindProvince("iran_kharg");
  CHECK(kharg != kNoIndex);
  s.StepDay();
  const Fixed64 before = s.Income("iran");
  s.SetDamage(kharg, Fixed64::FromRatio(1, 2));
  s.StepDay();
  CHECK(s.Income("iran") < before);
  CHECK(s.Income("iran") == Fixed64::FromRatio(145, 2));  // 95 - 45/2
  s.SetDamage(kharg, Units(1));
  s.StepDay();
  CHECK(s.Income("iran") == Units(50));
}

TEST(strat_capture_changes_owner_and_income) {
  havoc::World w(1);
  strat::Strategic s;
  const std::size_t kharg = s.Map().FindProvince("iran_kharg");
  s.StepDay();
  CHECK(s.Capture(kharg, "coalition", &w));
  CHECK(!s.Capture(kharg, "coalition", &w));  // already theirs
  CHECK(!s.Capture(kharg, "nobody", &w));
  CHECK(HasEvent(w, strat::kEvProvinceCaptured));
  CHECK(s.Provinces().at(kharg).owner == "coalition");
  s.StepDay();
  CHECK(s.Income("iran") == Units(50));
  CHECK(s.Provinces().at(kharg).control < Units(1));
  StepDays(s, 20);
  CHECK(s.Provinces().at(kharg).control == Units(1));
}

TEST(strat_commands_via_world) {
  havoc::ClockConfig cfg;
  cfg.ticksPerGameHour = 1;  // a game-day is 24 ticks
  havoc::World w(7, cfg);
  havoc::ops::Theater theater;  // both layers share the World's command handlers
  theater.Attach(w);
  const std::size_t node = theater.AddNode("kharg_north_beach", false);
  const std::size_t brigade = theater.AddBrigade("meu_1", "coalition_marine_expeditionary_unit", node);
  strat::Strategic s;
  s.Attach(w);
  const strat::WorldMap& m = s.Map();
  const auto iran = static_cast<std::int64_t>(m.FindFaction("iran"));
  const auto hormuz = static_cast<std::int64_t>(Zone(m, "hormuz"));

  w.ApplyCommand(havoc::Command{0, strat::kCmdBlockade, static_cast<std::uint64_t>(iran), hormuz, 0});
  CHECK(s.Zones().at(static_cast<std::size_t>(hormuz)).blockadedBy == "iran");
  CHECK(HasEvent(w, strat::kEvBlockadeStarted));
  for (int i = 0; i < 24; ++i) w.Step();  // one game-day
  CHECK(HasEvent(w, strat::kEvRouteClosed));
  CHECK(s.Income("coalition") == Units(8));

  // Wrong faction cannot lift; bad indices are ignored.
  const auto coalition = static_cast<std::uint64_t>(m.FindFaction("coalition"));
  w.ApplyCommand(havoc::Command{0, strat::kCmdLiftBlockade, coalition, hormuz, 0});
  CHECK(s.Zones().at(static_cast<std::size_t>(hormuz)).blockadedBy == "iran");
  w.ApplyCommand(havoc::Command{0, strat::kCmdBlockade, 99, hormuz, 0});
  w.ApplyCommand(havoc::Command{0, strat::kCmdBlockade, coalition, 999, 0});

  w.ApplyCommand(havoc::Command{0, strat::kCmdLiftBlockade, static_cast<std::uint64_t>(iran), hormuz, 0});
  CHECK(HasEvent(w, strat::kEvBlockadeLifted));
  for (int i = 0; i < 24; ++i) w.Step();
  CHECK(HasEvent(w, strat::kEvRouteOpened));
  CHECK(s.Income("coalition") == Units(240));

  // The operational layer still gets its own commands.
  w.ApplyCommand(havoc::Command{0, havoc::ops::kCmdSetPosture, brigade,
                                static_cast<std::int64_t>(havoc::ops::Posture::Rest), 0});
  CHECK(theater.Brigades().at(brigade).posture == havoc::ops::Posture::Rest);
}

TEST(strat_threat_drives_ops_route_interdiction) {
  strat::Strategic s;
  havoc::ops::Theater t;
  const std::size_t a = t.AddNode("port", true);
  const std::size_t b = t.AddNode("front", false);
  const std::size_t route = t.AddRoute(a, b, Units(400));
  CHECK(s.Blockade("iran", Zone(s.Map(), "bab_el_mandeb")));
  StepDays(s, 2);  // threat rises 1/4 per day
  CHECK(strat::LinkInterdiction(s, "bab_el_mandeb", t, route));
  CHECK(t.Routes().at(route).interdiction == Fixed64::FromRatio(1, 2));
  CHECK(!strat::LinkInterdiction(s, "no_such_zone", t, route));
  CHECK(t.Routes().at(route).interdiction == Fixed64::FromRatio(1, 2));
}

TEST(strat_deterministic_over_60_days) {
  auto run = [](bool hormuzFirst) {
    havoc::ClockConfig cfg;
    cfg.ticksPerGameHour = 2;  // 48 ticks per game-day
    havoc::World w(2026, cfg);
    strat::Strategic s;
    s.Attach(w);
    const strat::WorldMap& m = s.Map();
    const auto iran = static_cast<std::uint64_t>(m.FindFaction("iran"));
    const auto coalition = static_cast<std::uint64_t>(m.FindFaction("coalition"));
    const auto hormuz = static_cast<std::int64_t>(Zone(m, "hormuz"));
    const auto bab = static_cast<std::int64_t>(Zone(m, "bab_el_mandeb"));
    const std::uint64_t perDay = w.GetClock().TicksPerDay();
    for (std::uint64_t i = 0; i < 60 * perDay; ++i) {
      if (i == 5 * perDay) w.ApplyCommand(havoc::Command{i, strat::kCmdBlockade, iran, hormuzFirst ? hormuz : bab, 0});
      if (i == 20 * perDay) w.ApplyCommand(havoc::Command{i, strat::kCmdBlockade, coalition, bab, 0});
      if (i == 30 * perDay) w.ApplyCommand(havoc::Command{i, strat::kCmdLiftBlockade, iran, hormuzFirst ? hormuz : bab, 0});
      if (i == 40 * perDay) s.SetDamage(s.Map().FindProvince("iran_kharg"), Fixed64::FromRatio(3, 10));
      w.Step();
    }
    return s.Hash() ^ (havoc::HashWorld(w) * 31);
  };
  const auto a = run(true);
  const auto b = run(true);
  const auto c = run(false);
  CHECK(a == b);
  CHECK(a != c);
}
