// Core tests (fixed-point, RNG, clock, replay) and the test runner entry point.
// Module tests live in tests/<module>_tests.cpp; the runner is in TestHarness.h.
#include <cstdio>
#include <limits>

#include "TestHarness.h"
#include "havoc/Clock.h"
#include "havoc/Fixed.h"
#include "havoc/Random.h"
#include "havoc/Replay.h"
#include "havoc/StateHash.h"
#include "havoc/World.h"

namespace {

using namespace havoc;

TEST(fixed_arithmetic) {
  Fixed64 a = Fixed64::FromInt(3), b = Fixed64::FromInt(4);
  CHECK((a + b) == Fixed64::FromInt(7));
  CHECK((a - b) == Fixed64::FromInt(-1));
  CHECK((a * b) == Fixed64::FromInt(12));
  CHECK((Fixed64::FromInt(12) / b) == a);
  CHECK(a < b);
  CHECK(b >= a);
  CHECK(a != b);
  CHECK(Fixed64::FromRatio(1, 2) + Fixed64::FromRatio(1, 2) == Fixed64::FromInt(1));
  CHECK(Fixed64::FromRatio(3, 2) * Fixed64::FromInt(2) == Fixed64::FromInt(3));
  CHECK(Fixed64::FromInt(-5).ToIntFloor() == -5);
}

TEST(fixed_rounding) {
  Fixed64 h = Fixed64::FromRatio(5, 2);   // 2.5
  CHECK(h.ToIntFloor() == 2);
  CHECK(h.ToIntTrunc() == 2);
  CHECK(h.ToIntRound() == 3);
  Fixed64 n = Fixed64::FromRatio(-5, 2);  // -2.5
  CHECK(n.ToIntFloor() == -3);
  CHECK(n.ToIntTrunc() == -2);
  CHECK(n.ToIntRound() == -2);
  // 1/3 truncates toward zero; 3 * (1/3) is just below 1.
  Fixed64 t = Fixed64::FromRatio(1, 3);
  CHECK(t * Fixed64::FromInt(3) == Fixed64::FromRaw(Fixed64::kOne - 1));
  CHECK(Fixed64::FromRatio(-1, 3).raw == -t.raw);
}

// Golden checksum over mixed-sign multiply/divide/ratio results. The same test
// runs in havoc_sim_tests_portable (no __int128), so both code paths must agree
// bit for bit with this constant.
TEST(fixed_math_golden) {
  Pcg32 rng(20261104);
  std::uint64_t h = 1469598103934665603ull;
  auto mix = [&h](std::int64_t v) { h ^= static_cast<std::uint64_t>(v); h *= 1099511628211ull; };
  auto next64 = [&rng]() {
    const std::uint64_t hi = rng.NextU32(), lo = rng.NextU32();
    return static_cast<std::int64_t>((hi << 32) | lo);
  };
  for (int i = 0; i < 200000; ++i) {
    std::int64_t a = next64(), b = next64();
    if (i % 3 == 0) b >>= 20;  // include small magnitudes
    if (i % 5 == 0) a >>= 33;
    mix((Fixed64::FromRaw(a) * Fixed64::FromRaw(b)).raw);
    if (b != 0) mix((Fixed64::FromRaw(a) / Fixed64::FromRaw(b)).raw);
    if (b != 0) mix(Fixed64::FromRatio(a >> 31, b).raw);
  }
  std::printf("    golden=%llu\n", static_cast<unsigned long long>(h));
  CHECK(h == 8738330279699673661ull);
}

TEST(rng_same_seed_same_sequence) {
  Pcg32 a(12345), b(12345), c(54321);
  bool differs = false;
  for (int i = 0; i < 1000; ++i) {
    std::uint32_t x = a.NextU32();
    CHECK(x == b.NextU32());
    if (x != c.NextU32()) differs = true;
  }
  CHECK(differs);
  Pcg32 d(7);
  for (int i = 0; i < 1000; ++i) CHECK(d.NextBounded(10) < 10);
}

TEST(clock_callback_counts) {
  ClockConfig cfg;
  cfg.ticksPerGameHour = 10;
  cfg.hoursPerGameDay = 24;
  Clock c(cfg);
  int tac = 0, op = 0, strat = 0;
  c.OnTactical([&](std::uint64_t) { ++tac; });
  c.OnOperational([&](std::uint64_t) { ++op; });
  c.OnStrategic([&](std::uint64_t) { ++strat; });
  for (int i = 0; i < 10 * 24 * 3; ++i) c.Advance();  // 3 game-days
  CHECK(tac == 720);
  CHECK(op == 72);
  CHECK(strat == 3);
  CHECK(c.Tick() == 720);
  CHECK(ClockConfig::kTacticalHz == 20);
}

constexpr ComponentId kValue = 1;
constexpr std::uint32_t kEvTick = 1, kEvDay = 2;

void Setup(World& w) {
  for (int i = 0; i < 8; ++i) w.Components().Set(kValue, w.CreateEntity(), i);
  w.RegisterSystem(Layer::Tactical, [](World& world) {
    for (const auto& kv : world.Components().All(kValue)) {
      std::int64_t add = world.Rng().NextBounded(10);
      world.Components().Set(kValue, kv.first, kv.second + add);
    }
  });
  w.RegisterSystem(Layer::Operational, [](World& world) { world.Emit(kEvTick, 0, static_cast<std::int64_t>(world.Tick())); });
  w.RegisterSystem(Layer::Strategic, [](World& world) { world.Emit(kEvDay, 0, static_cast<std::int64_t>(world.Tick())); });
  w.SetCommandHandler([](World& world, const Command& c) {
    if (c.kind == 1) world.Components().Set(kValue, c.entity, c.b);
  });
}

Recording MakeRec(std::int64_t cmdValue) {
  Recording r;
  r.seed = 42;
  r.ticks = 10000;
  r.commands.push_back(Command{100, 1, 3, 0, 500});
  r.commands.push_back(Command{5000, 1, 4, 0, cmdValue});
  return r;
}

TEST(same_seed_same_commands_same_hash) {
  Recording a = MakeRec(900), b = MakeRec(900);
  CHECK(Run(a, Setup) == Run(b, Setup));
}

TEST(different_command_different_hash) {
  CHECK(Run(MakeRec(900), Setup) != Run(MakeRec(901), Setup));
  Recording other = MakeRec(900);
  other.seed = 43;
  CHECK(Run(MakeRec(900), Setup) != Run(other, Setup));
}

TEST(replay_matches) {
  Recording r = MakeRec(900);
  Record(r, Setup);
  CHECK(r.finalHash != 0);
  CHECK(Verify(r, Setup));
  r.commands[1].b = 1;  // tampered recording must not verify
  CHECK(!Verify(r, Setup));
}

}  // namespace

int main() {
  for (const havoc_test::TestCase& t : havoc_test::Tests()) {
    int before = havoc_test::g_failures;
    t.fn();
    std::printf("[%s] %s\n", havoc_test::g_failures == before ? " OK " : "FAIL", t.name);
  }
  std::printf("%zu tests, %d failed checks\n", havoc_test::Tests().size(), havoc_test::g_failures);
  return havoc_test::g_failures == 0 ? 0 : 1;
}
