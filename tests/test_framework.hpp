#pragma once

#include <exception>
#include <functional>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace spencer::test {

struct Case final {
  std::string name;
  std::function<void()> run;
};

inline std::vector<Case>& registry() {
  static std::vector<Case> cases;
  return cases;
}

class Registrar final {
 public:
  Registrar(std::string name, std::function<void()> run) {
    registry().push_back(Case{std::move(name), std::move(run)});
  }
};

[[noreturn]] inline void fail(const std::string_view expression, const std::string_view file,
                              const int line, const std::string_view detail = {}) {
  std::ostringstream message;
  message << file << ':' << line << ": assertion failed: " << expression;
  if (!detail.empty()) {
    message << " (" << detail << ')';
  }
  throw std::runtime_error(message.str());
}

inline int run_all() {
  std::size_t failures = 0;
  for (const auto& test_case : registry()) {
    try {
      test_case.run();
      std::cout << "PASS " << test_case.name << '\n';
    } catch (const std::exception& error) {
      ++failures;
      std::cerr << "FAIL " << test_case.name << " — " << error.what() << '\n';
    } catch (...) {
      ++failures;
      std::cerr << "FAIL " << test_case.name << " — unknown exception\n";
    }
  }
  std::cout << registry().size() << " test case(s), " << failures << " failure(s)\n";
  return failures == 0 ? 0 : 1;
}

}  // namespace spencer::test

#define SPENCER_TEST(name)                                                        \
  static void name();                                                             \
  static const ::spencer::test::Registrar name##_registrar{#name, name};         \
  static void name()

#define SPENCER_REQUIRE(expression)                                               \
  do {                                                                            \
    if (!(expression)) {                                                          \
      ::spencer::test::fail(#expression, __FILE__, __LINE__);                    \
    }                                                                             \
  } while (false)

#define SPENCER_REQUIRE_MESSAGE(expression, detail)                               \
  do {                                                                            \
    if (!(expression)) {                                                          \
      ::spencer::test::fail(#expression, __FILE__, __LINE__, detail);            \
    }                                                                             \
  } while (false)
