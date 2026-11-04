#include "havoc/StateHash.h"

namespace havoc {
namespace {

struct Fnv {
  std::uint64_t h = 14695981039346656037ULL;
  void Byte(std::uint8_t b) { h ^= b; h *= 1099511628211ULL; }
  void U64(std::uint64_t v) {
    for (int i = 0; i < 8; ++i) Byte(static_cast<std::uint8_t>(v >> (8 * i)));  // little-endian
  }
  void I64(std::int64_t v) { U64(static_cast<std::uint64_t>(v)); }
};

}  // namespace

std::uint64_t HashWorld(const World& w) {
  Fnv f;
  f.U64(w.Tick());
  f.U64(w.Seed());
  f.U64(w.Rng().State());
  f.U64(w.Rng().Inc());
  f.U64(w.NextEntityId());
  for (const auto& comp : w.Components().Data()) {
    f.U64(comp.first);
    f.U64(comp.second.size());
    for (const auto& kv : comp.second) {
      f.U64(kv.first);
      f.I64(kv.second);
    }
  }
  f.U64(w.Log().Size());
  for (const Event& e : w.Log().Events()) {
    f.U64(e.tick);
    f.U64(e.kind);
    f.U64(e.entity);
    f.I64(e.a);
    f.I64(e.b);
  }
  return f.h;
}

}  // namespace havoc
