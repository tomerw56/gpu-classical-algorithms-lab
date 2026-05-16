#pragma once

#include <cmath>
#include <exception>
#include <iostream>
#include <sstream>
#include <string>

namespace algobench::test
{

inline int& failure_count()
{
    static int count = 0;
    return count;
}

inline void fail(const char* file, int line, const std::string& message)
{
    ++failure_count();
    std::cerr << file << ":" << line << ": test failure: " << message << "\n";
}

inline int finish()
{
    if (failure_count() == 0)
    {
        std::cout << "all checks passed\n";
        return 0;
    }

    std::cerr << failure_count() << " check(s) failed\n";
    return 1;
}

} // namespace algobench::test

#define TEST_CHECK(condition)                                                                 \
    do                                                                                        \
    {                                                                                         \
        if (!(condition))                                                                      \
        {                                                                                     \
            ::algobench::test::fail(__FILE__, __LINE__, "condition failed: " #condition);     \
        }                                                                                     \
    } while (0)

#define TEST_CHECK_EQ(actual, expected)                                                       \
    do                                                                                        \
    {                                                                                         \
        const auto actual_value__ = (actual);                                                  \
        const auto expected_value__ = (expected);                                              \
        if (!(actual_value__ == expected_value__))                                             \
        {                                                                                     \
            std::ostringstream os__;                                                          \
            os__ << "expected " #actual " == " #expected << ", got "                         \
                 << actual_value__ << " vs " << expected_value__;                             \
            ::algobench::test::fail(__FILE__, __LINE__, os__.str());                          \
        }                                                                                     \
    } while (0)

#define TEST_CHECK_NEAR(actual, expected, tolerance)                                          \
    do                                                                                        \
    {                                                                                         \
        const double actual_value__ = static_cast<double>(actual);                             \
        const double expected_value__ = static_cast<double>(expected);                         \
        const double tolerance_value__ = static_cast<double>(tolerance);                       \
        if (std::abs(actual_value__ - expected_value__) > tolerance_value__)                   \
        {                                                                                     \
            std::ostringstream os__;                                                          \
            os__ << "expected " #actual " near " #expected << ", got "                       \
                 << actual_value__ << " vs " << expected_value__                              \
                 << " tolerance " << tolerance_value__;                                       \
            ::algobench::test::fail(__FILE__, __LINE__, os__.str());                          \
        }                                                                                     \
    } while (0)

#define TEST_CHECK_THROWS(statement)                                                          \
    do                                                                                        \
    {                                                                                         \
        bool threw__ = false;                                                                  \
        try                                                                                   \
        {                                                                                     \
            statement;                                                                         \
        }                                                                                     \
        catch (const std::exception&)                                                         \
        {                                                                                     \
            threw__ = true;                                                                    \
        }                                                                                     \
        catch (...)                                                                           \
        {                                                                                     \
            threw__ = true;                                                                    \
        }                                                                                     \
        if (!threw__)                                                                         \
        {                                                                                     \
            ::algobench::test::fail(__FILE__, __LINE__, "expected exception: " #statement);    \
        }                                                                                     \
    } while (0)
