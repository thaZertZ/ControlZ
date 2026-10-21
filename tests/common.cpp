#include <iostream>
#include "../src/headers/Common.hpp"
#include "test_support.hpp"

int main() {
    using namespace ControlZ;

    static_assert(AllSameAs<int, int, int>);
    static_assert(!AllSameAs<int, int, long>);
    static_assert(network_byte_order_copy<std::uint16_t>(0x1234) == 0x3412);
    std::string a  = "123";
    auto y = network_byte_order_copy(a);
    REQUIRE(network_byte_order_copy(a) == "321");
    std::vector<std::uint16_t> b = {0x1234, 0x5678};
    std::vector<std::uint16_t> x = {0x3412, 0x7856};
    REQUIRE(network_byte_order_copy(b) == x);

    REQUIRE(random_number() != random_number());
    return 0;
}
