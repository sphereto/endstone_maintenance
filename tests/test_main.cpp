#include "test_framework.h"

#include <iostream>

namespace test {

std::vector<Case> &registry()
{
    static std::vector<Case> cases;
    return cases;
}

int &failures()
{
    static int count = 0;
    return count;
}

} // namespace test

int main()
{
    int passed = 0;
    for (const auto &c : test::registry()) {
        const int before = test::failures();
        c.fn();
        if (test::failures() == before) {
            ++passed;
            std::cout << "[ PASS ] " << c.name << '\n';
        }
        else {
            std::cout << "[ FAIL ] " << c.name << '\n';
        }
    }
    std::cout << passed << "/" << test::registry().size() << " tests passed\n";
    return test::failures() == 0 ? 0 : 1;
}
