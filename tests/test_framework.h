// Simple Test Framework for avioflow
// Inspired by FFmpeg FATE testing style - no external dependencies

#pragma once

#include <chrono>
#include <cmath>
#include <functional>
#include <iostream>
#include <string>
#include <vector>


namespace avioflow::test {

// Test result statistics
struct TestStats {
  int passed = 0;
  int failed = 0;
  int skipped = 0;
};

// ANSI color codes for terminal output
#ifdef _WIN32
#define COLOR_RESET ""
#define COLOR_GREEN ""
#define COLOR_RED ""
#define COLOR_YELLOW ""
#define COLOR_CYAN ""
#else
#define COLOR_RESET "\033[0m"
#define COLOR_GREEN "\033[32m"
#define COLOR_RED "\033[31m"
#define COLOR_YELLOW "\033[33m"
#define COLOR_CYAN "\033[36m"
#endif

// Assertion macros
#define TEST_ASSERT(condition, message)                                        \
  do {                                                                         \
    if (!(condition)) {                                                        \
      std::cerr << COLOR_RED << "[ASSERT FAIL] " << COLOR_RESET << __FILE__    \
                << ":" << __LINE__ << " - " << message << std::endl;           \
      return false;                                                            \
    }                                                                          \
  } while (0)

#define TEST_ASSERT_EQ(expected, actual, name)                                 \
  do {                                                                         \
    auto _exp = (expected);                                                    \
    auto _act = (actual);                                                      \
    if (_exp != _act) {                                                        \
      std::cerr << COLOR_RED << "[ASSERT FAIL] " << COLOR_RESET << name        \
                << ": expected " << _exp << ", got " << _act << std::endl;     \
      return false;                                                            \
    }                                                                          \
  } while (0)

#define TEST_ASSERT_NEAR(expected, actual, tolerance, name)                    \
  do {                                                                         \
    auto _exp = (expected);                                                    \
    auto _act = (actual);                                                      \
    if (std::abs(_exp - _act) > (tolerance)) {                                 \
      std::cerr << COLOR_RED << "[ASSERT FAIL] " << COLOR_RESET << name        \
                << ": expected ~" << _exp << ", got " << _act                  \
                << " (tolerance: " << (tolerance) << ")" << std::endl;         \
      return false;                                                            \
    }                                                                          \
  } while (0)

#define TEST_ASSERT_NOT_NULL(ptr, name)                                        \
  do {                                                                         \
    if ((ptr) == nullptr) {                                                    \
      std::cerr << COLOR_RED << "[ASSERT FAIL] " << COLOR_RESET << name        \
                << " is null" << std::endl;                                    \
      return false;                                                            \
    }                                                                          \
  } while (0)

#define TEST_ASSERT_GT(value, threshold, name)                                 \
  do {                                                                         \
    auto _val = (value);                                                       \
    auto _thr = (threshold);                                                   \
    if (_val <= _thr) {                                                        \
      std::cerr << COLOR_RED << "[ASSERT FAIL] " << COLOR_RESET << name        \
                << ": " << _val << " is not > " << _thr << std::endl;          \
      return false;                                                            \
    }                                                                          \
  } while (0)

// Test case structure
struct TestCase {
  std::string name;
  std::function<bool()> func;
  bool skip = false;
  std::string skip_reason;
};

// Test runner
class TestRunner {
public:
  void add_test(const std::string &name, std::function<bool()> func) {
    tests_.push_back({name, func, false, ""});
  }

  void add_test_skip(const std::string &name, std::function<bool()> func,
                     const std::string &reason) {
    tests_.push_back({name, func, true, reason});
  }

  TestStats run_all() {
    TestStats stats;

    std::cout << "\n"
              << COLOR_CYAN
              << "========================================" << COLOR_RESET
              << std::endl;
    std::cout << COLOR_CYAN << "  Running " << tests_.size() << " test(s)"
              << COLOR_RESET << std::endl;
    std::cout << COLOR_CYAN
              << "========================================" << COLOR_RESET
              << "\n"
              << std::endl;

    for (const auto &test : tests_) {
      std::cout << "  " << test.name << " ... ";
      std::cout.flush();

      if (test.skip) {
        std::cout << COLOR_YELLOW << "[SKIP] " << test.skip_reason
                  << COLOR_RESET << std::endl;
        stats.skipped++;
        continue;
      }

      auto start = std::chrono::high_resolution_clock::now();
      bool result = false;

      try {
        result = test.func();
      } catch (const std::exception &e) {
        std::cout << COLOR_RED << "[EXCEPTION] " << e.what() << COLOR_RESET
                  << std::endl;
        stats.failed++;
        continue;
      } catch (...) {
        std::cout << COLOR_RED << "[EXCEPTION] Unknown error" << COLOR_RESET
                  << std::endl;
        stats.failed++;
        continue;
      }

      auto end = std::chrono::high_resolution_clock::now();
      auto duration =
          std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

      if (result) {
        std::cout << COLOR_GREEN << "[PASS]" << COLOR_RESET << " ("
                  << duration.count() << "ms)" << std::endl;
        stats.passed++;
      } else {
        std::cout << COLOR_RED << "[FAIL]" << COLOR_RESET << std::endl;
        stats.failed++;
      }
    }

    // Summary
    std::cout << "\n"
              << COLOR_CYAN
              << "========================================" << COLOR_RESET
              << std::endl;
    std::cout << "  Results: ";
    std::cout << COLOR_GREEN << stats.passed << " passed" << COLOR_RESET
              << ", ";
    std::cout << COLOR_RED << stats.failed << " failed" << COLOR_RESET << ", ";
    std::cout << COLOR_YELLOW << stats.skipped << " skipped" << COLOR_RESET
              << std::endl;
    std::cout << COLOR_CYAN
              << "========================================" << COLOR_RESET
              << "\n"
              << std::endl;

    return stats;
  }

private:
  std::vector<TestCase> tests_;
};

} // namespace avioflow::test
