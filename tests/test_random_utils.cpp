#include "common/random_utils.hpp"

#include "test_support.hpp"

#include <algorithm>

int main()
{
    const auto doubles_a = algobench::random_double_vector(16, -1.0, 1.0, 42u);
    const auto doubles_b = algobench::random_double_vector(16, -1.0, 1.0, 42u);
    const auto doubles_c = algobench::random_double_vector(16, -1.0, 1.0, 43u);

    TEST_CHECK_EQ(doubles_a.size(), static_cast<std::size_t>(16));
    TEST_CHECK(doubles_a == doubles_b);
    TEST_CHECK(doubles_a != doubles_c);
    TEST_CHECK(std::all_of(doubles_a.begin(), doubles_a.end(), [](double value) {
        return value >= -1.0 && value <= 1.0;
    }));

    const auto ints_a = algobench::random_int_vector(32, 10, 20, 77u);
    const auto ints_b = algobench::random_int_vector(32, 10, 20, 77u);
    const auto ints_c = algobench::random_int_vector(32, 10, 20, 78u);

    TEST_CHECK_EQ(ints_a.size(), static_cast<std::size_t>(32));
    TEST_CHECK(ints_a == ints_b);
    TEST_CHECK(ints_a != ints_c);
    TEST_CHECK(std::all_of(ints_a.begin(), ints_a.end(), [](int value) {
        return value >= 10 && value <= 20;
    }));

    return algobench::test::finish();
}
