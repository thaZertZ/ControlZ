#include <iostream>
#include "../../src/headers/Common.hpp"
#include "test_support.hpp"

int main() {
    using namespace ControlZ;

    static_assert(AllSameAs<int, int, int>);
    static_assert(!AllSameAs<int, int, long>);
    static_assert(network_byte_order_copy<std::uint16_t>(0x1234) == 0x3412);

    REQUIRE(random_number() != random_number());
    return 0;
}
