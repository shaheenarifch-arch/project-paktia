#pragma once
// Seeded PCG32 (XSH-RR). No std::random_device, no time.
#include <cstdint>

namespace havoc {

class Pcg32 {
 public:
  explicit Pcg32(std::uint64_t seed = 0, std::uint64_t stream = 54) { Seed(seed, stream); }

  void Seed(std::uint64_t seed, std::uint64_t stream = 54) {
    state_ = 0;
    inc_ = (stream << 1u) | 1u;
    NextU32();
    state_ += seed;
    NextU32();
  }

  std::uint32_t NextU32() {
    std::uint64_t old = state_;
    state_ = old * 6364136223846793005ULL + inc_;
    std::uint32_t xorshifted = static_cast<std::uint32_t>(((old >> 18u) ^ old) >> 27u);
    std::uint32_t rot = static_cast<std::uint32_t>(old >> 59u);
    return (xorshifted >> rot) | (xorshifted << ((~rot + 1u) & 31u));
  }

  // Uniform in [0, bound), unbiased. bound must be > 0.
  std::uint32_t NextBounded(std::uint32_t bound) {
    std::uint32_t threshold = (~bound + 1u) % bound;
    for (;;) {
      std::uint32_t r = NextU32();
      if (r >= threshold) return r % bound;
    }
  }

  std::uint64_t State() const { return state_; }
  std::uint64_t Inc() const { return inc_; }

 private:
  std::uint64_t state_ = 0;
  std::uint64_t inc_ = 0;
};

}  // namespace havoc
