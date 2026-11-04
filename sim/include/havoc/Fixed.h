#pragma once
// 32.32 signed fixed-point. No floating point anywhere in the sim.
// Rounding: multiplication and division truncate toward zero.
#include <cstdint>

namespace havoc {

struct Fixed64 {
  std::int64_t raw = 0;  // value * 2^32

  static constexpr int kFracBits = 32;
  static constexpr std::int64_t kOne = std::int64_t{1} << kFracBits;

  constexpr Fixed64() = default;

  static constexpr Fixed64 FromRaw(std::int64_t r) { Fixed64 f; f.raw = r; return f; }
  static constexpr Fixed64 FromInt(std::int32_t v) { return FromRaw(static_cast<std::int64_t>(v) * kOne); }
  // num/den, truncated toward zero. den must be non-zero.
  static constexpr Fixed64 FromRatio(std::int64_t num, std::int64_t den) {
    return FromRaw(static_cast<std::int64_t>((static_cast<__int128>(num) << kFracBits) / den));
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
    return FromRaw(static_cast<std::int64_t>((static_cast<__int128>(a.raw) * b.raw) >> kFracBits));
  }
  friend constexpr Fixed64 operator/(Fixed64 a, Fixed64 b) {
    return FromRaw(static_cast<std::int64_t>((static_cast<__int128>(a.raw) << kFracBits) / b.raw));
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
