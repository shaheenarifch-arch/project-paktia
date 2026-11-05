#pragma once
// Self-contained mini test runner (no external framework). Each module has its
// own tests/<module>_tests.cpp that includes this header and uses TEST/CHECK;
// tests register themselves, and main.cpp runs them.
#include <cstdio>
#include <functional>
#include <utility>
#include <vector>

namespace havoc_test {

struct TestCase { const char* name; std::function<void()> fn; };
inline std::vector<TestCase>& Tests() { static std::vector<TestCase> t; return t; }
inline int g_failures = 0;

struct Reg { Reg(const char* n, std::function<void()> f) { Tests().push_back({n, std::move(f)}); } };

}  // namespace havoc_test

#define TEST(name) \
  static void name(); \
  static havoc_test::Reg reg_##name(#name, name); \
  static void name()

#define CHECK(cond) \
  do { if (!(cond)) { std::printf("    CHECK failed %s:%d: %s\n", __FILE__, __LINE__, #cond); ++havoc_test::g_failures; } } while (0)
