#pragma once
// 32.32 signed fixed-point. No floating point anywhere in the sim.
// Rounding: multiplication floors (arithmetic shift); division truncates toward zero.
#include <cstdint>

// 128-bit intermediates: GCC/Clang use the built-in __int128. MSVC (used by
// Unreal on Windows) has no __int128, so it gets a portable path that gives
// bit-identical results. Define HAVOC_FORCE_PORTABLE_I128 to test that path
// on GCC/Clang.
#if defined(__SIZEOF_INT128__) && !defined(HAVOC_FORCE_PORTABLE_I128)
#define HAVOC_HAS_INT128 1
#else
#define HAVOC_HAS_INT128 0
#endif

namespace havoc {
namespace detail {

#if HAVOC_HAS_INT128
// (a * b) >> 32 with an arithmetic shift (floors negative results).
constexpr std::int64_t MulShift32(std::int64_t a, std::int64_t b) {
  return static_cast<std::int64_t>((static_cast<__int128>(a) * b) >> 32);
}
// (a << 32) / b, truncated toward zero. b must be non-zero.
constexpr std::int64_t ShiftDiv32(std::int64_t a, std::int64_t b) {
  return static_cast<std::int64_t>((static_cast<__int128>(a) << 32) / b);
}
#else
struct U128 { std::uint64_t hi = 0, lo = 0; };

constexpr std::uint64_t Abs64(std::int64_t v) {
  return v < 0 ? std::uint64_t{0} - static_cast<std::uint64_t>(v) : static_cast<std::uint64_t>(v);
}

constexpr U128 Neg128(U128 v) {
  U128 r{~v.hi, ~v.lo};
  r.lo += 1;
  if (r.lo == 0) r.hi += 1;
  return r;
}

constexpr U128 MulU64(std::uint64_t a, std::uint64_t b) {
  const std::uint64_t a0 = a & 0xFFFFFFFFu, a1 = a >> 32;
  const std::uint64_t b0 = b & 0xFFFFFFFFu, b1 = b >> 32;
  const std::uint64_t p00 = a0 * b0, p01 = a0 * b1, p10 = a1 * b0, p11 = a1 * b1;
  const std::uint64_t mid = (p00 >> 32) + (p01 & 0xFFFFFFFFu) + (p10 & 0xFFFFFFFFu);
  U128 r;
  r.lo = (mid << 32) | (p00 & 0xFFFFFFFFu);
  r.hi = p11 + (p01 >> 32) + (p10 >> 32) + (mid >> 32);
  return r;
}

// Unsigned 128 / 64 long division; returns the low 64 bits of the quotient.
constexpr std::uint64_t DivU128(U128 n, std::uint64_t d) {
  std::uint64_t q = 0, rem = 0;
  for (int i = 127; i >= 0; --i) {
    const std::uint64_t bit = i >= 64 ? (n.hi >> (i - 64)) & 1u : (n.lo >> i) & 1u;
    const std::uint64_t carry = rem >> 63;
    rem = (rem << 1) | bit;
    if (carry != 0 || rem >= d) {
      rem -= d;
      if (i < 64) q |= std::uint64_t{1} << i;
    }
  }
  return q;
}

constexpr std::int64_t MulShift32(std::int64_t a, std::int64_t b) {
  const bool neg = (a < 0) != (b < 0);
  U128 p = MulU64(Abs64(a), Abs64(b));
  if (neg) p = Neg128(p);
  return static_cast<std::int64_t>((p.lo >> 32) | (p.hi << 32));
}

constexpr std::int64_t ShiftDiv32(std::int64_t a, std::int64_t b) {
  const bool neg = (a < 0) != (b < 0);
  const std::uint64_t ua = Abs64(a);
  const std::uint64_t q = DivU128(U128{ua >> 32, ua << 32}, Abs64(b));
  return static_cast<std::int64_t>(neg ? std::uint64_t{0} - q : q);
}
#endif

}  // namespace detail

struct Fixed64 {
  std::int64_t raw = 0;  // value * 2^32

  static constexpr int kFracBits = 32;
  static constexpr std::int64_t kOne = std::int64_t{1} << kFracBits;

  constexpr Fixed64() = default;

  static constexpr Fixed64 FromRaw(std::int64_t r) { Fixed64 f; f.raw = r; return f; }
  static constexpr Fixed64 FromInt(std::int32_t v) { return FromRaw(static_cast<std::int64_t>(v) * kOne); }
  // num/den, truncated toward zero. den must be non-zero.
  static constexpr Fixed64 FromRatio(std::int64_t num, std::int64_t den) {
    return FromRaw(detail::ShiftDiv32(num, den));
  }

  // Integer part rounded toward negative infinity.
  constexpr std::int64_t ToIntFloor() const { return raw >> kFracBits; }
  // Integer part truncated toward zero.
  constexpr std::int64_t ToIntTrunc() const { return raw / kOne; }
  // Nearest integer, halves rounded up.
  constexpr std::int64_t ToIntRound() const { return (raw + (kOne >> 1)) >> kFracBits; }

  friend constexpr Fixed64 operator+(Fixed64 a, Fixed64 b) { return FromRaw(a.raw + b.raw); }
  friend constexpr Fixed64 operator-(Fixed64 a, Fixed64 b) { return FromRaw(a.raw - b.raw); }
  friend constexpr Fixed64 operator-(Fixed64 a) { return FromRaw(-a.raw); }
  friend constexpr Fixed64 operator*(Fixed64 a, Fixed64 b) {
    return FromRaw(detail::MulShift32(a.raw, b.raw));
  }
  friend constexpr Fixed64 operator/(Fixed64 a, Fixed64 b) {
    return FromRaw(detail::ShiftDiv32(a.raw, b.raw));
  }
  constexpr Fixed64& operator+=(Fixed64 o) { raw += o.raw; return *this; }
  constexpr Fixed64& operator-=(Fixed64 o) { raw -= o.raw; return *this; }

  friend constexpr bool operator==(Fixed64 a, Fixed64 b) { return a.raw == b.raw; }
  friend constexpr bool operator!=(Fixed64 a, Fixed64 b) { return a.raw != b.raw; }
  friend constexpr bool operator<(Fixed64 a, Fixed64 b) { return a.raw < b.raw; }
  friend constexpr bool operator<=(Fixed64 a, Fixed64 b) { return a.raw <= b.raw; }
  friend constexpr bool operator>(Fixed64 a, Fixed64 b) { return a.raw > b.raw; }
  friend constexpr bool operator>=(Fixed64 a, Fixed64 b) { return a.raw >= b.raw; }
};

}  // namespace havoc
