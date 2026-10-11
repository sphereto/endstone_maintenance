#pragma once

// Tiny dependency-free test harness so CI never has to download a test framework.

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

#define CHECK(expr)                                                                    \
    do {                                                                               \
        if (!(expr)) {                                                                 \
            ++::test::failures();                                                      \
            std::cerr << __FILE__ << ":" << __LINE__ << ": CHECK failed: " #expr "\n"; \
        }                                                                              \
    } while (false)

#define CHECK_EQ(a, b)                                                                           \
    do {                                                                                         \
        const auto &va = (a);                                                                    \
        const auto &vb = (b);                                                                    \
        if (!(va == vb)) {                                                                       \
            ++::test::failures();                                                                \
            std::cerr << __FILE__ << ":" << __LINE__ << ": CHECK_EQ failed: " #a " != " #b "\n"; \
        }                                                                                        \
    } while (false)
