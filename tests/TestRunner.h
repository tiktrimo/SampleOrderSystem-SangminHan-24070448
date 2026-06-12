#pragma once
#include <iostream>
#include <string>
#include <vector>
#include <functional>
#include <stdexcept>

struct TestCase {
    std::string name;
    std::function<void()> fn;
};

inline std::vector<TestCase>& getTests() {
    static std::vector<TestCase> tests;
    return tests;
}

struct TestRegistrar {
    TestRegistrar(const std::string& name, std::function<void()> fn) {
        getTests().push_back({ name, fn });
    }
};

#define TEST(name) \
    static void test_func_##name(); \
    static TestRegistrar reg_##name(#name, test_func_##name); \
    static void test_func_##name()

#define ASSERT_TRUE(expr) \
    do { if (!(expr)) throw std::runtime_error("ASSERT_TRUE 실패: " #expr " (line " + std::to_string(__LINE__) + ")"); } while(0)

#define ASSERT_FALSE(expr) \
    do { if (expr) throw std::runtime_error("ASSERT_FALSE 실패: " #expr " (line " + std::to_string(__LINE__) + ")"); } while(0)

#define ASSERT_EQ(a, b) \
    do { if ((a) != (b)) throw std::runtime_error(std::string("ASSERT_EQ 실패: " #a " != " #b " (line ") + std::to_string(__LINE__) + ")"); } while(0)

#define ASSERT_NE(a, b) \
    do { if ((a) == (b)) throw std::runtime_error(std::string("ASSERT_NE 실패: " #a " == " #b " (line ") + std::to_string(__LINE__) + ")"); } while(0)

#define ASSERT_GT(a, b) \
    do { if (!((a) > (b))) throw std::runtime_error(std::string("ASSERT_GT 실패: " #a " <= " #b " (line ") + std::to_string(__LINE__) + ")"); } while(0)

inline int runAllTests() {
    int passed = 0, failed = 0;
    for (auto& tc : getTests()) {
        try {
            tc.fn();
            std::cout << "[PASS] " << tc.name << "\n";
            ++passed;
        } catch (const std::exception& e) {
            std::cout << "[FAIL] " << tc.name << " -- " << e.what() << "\n";
            ++failed;
        }
    }
    std::cout << "\n===========================\n";
    std::cout << "결과: " << passed << " passed, " << failed << " failed\n";
    std::cout << "===========================\n";
    return failed > 0 ? 1 : 0;
}
