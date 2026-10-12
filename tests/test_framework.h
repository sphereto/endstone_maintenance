#pragma once

// Tiny dependency-free test harness so CI never has to download a test framework.
//
// The macros deliberately do not stringize their arguments (`#expr`): MSVC's default preprocessor does not escape
// raw string literals when stringizing, which breaks any check that mentions one. The file and line identify the check.

#include <functional>
#include <iostream>
#include <string>
#include <vector>

namespace test {

struct Case {
    std::string name;
    std::function<void()> fn;
};

std::vector<Case> &registry();
int &failures();

struct Registrar {
    Registrar(std::string name, std::function<void()> fn)
    {
        registry().push_back({std::move(name), std::move(fn)});
    }
};

} // namespace test

#define TEST(name)                                                 \
    static void test_##name();                                     \
    static ::test::Registrar registrar_##name(#name, test_##name); \
    static void test_##name()

#define CHECK(expr)                                                         \
    do {                                                                    \
        if (!(expr)) {                                                      \
            ++::test::failures();                                           \
            std::cerr << __FILE__ << ":" << __LINE__ << ": CHECK failed\n"; \
        }                                                                   \
    } while (false)

#define CHECK_EQ(a, b)                                                         \
    do {                                                                       \
        const auto &check_lhs = (a);                                           \
        const auto &check_rhs = (b);                                           \
        if (!(check_lhs == check_rhs)) {                                       \
            ++::test::failures();                                              \
            std::cerr << __FILE__ << ":" << __LINE__ << ": CHECK_EQ failed\n"; \
        }                                                                      \
    } while (false)
