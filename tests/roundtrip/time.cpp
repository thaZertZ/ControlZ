#include <iostream>
#include "../../src/headers/Time.hpp"
#include "test_support.hpp"

int main() {
    using namespace ControlZ;

    constexpr std::int64_t epoch = custom_epoch_seconds();
    static_assert(epoch > 0);
    REQUIRE(unix_to_custom(epoch) == 0);
    REQUIRE(unix_to_custom(epoch + 1) == 1);
    REQUIRE(unix_to_custom(epoch - 1) == 0);
    REQUIRE(custom_to_unix(0) == epoch);
    REQUIRE(custom_to_unix(unix_to_custom(epoch + 123456)) == epoch + 123456);
    REQUIRE(timestamp_now() == 0); // We haven't reached 2027 yet

    try {
        const std::int64_t sample = epoch + 86400;
        REQUIRE(local_to_utc(utc_to_local(sample)) == sample);
    } catch (const std::exception& exception) {
        std::cerr << "timezone database unavailable: " << exception.what() << '\n';
    }

    return 0;
}
