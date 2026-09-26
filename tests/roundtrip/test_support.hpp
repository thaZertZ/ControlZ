#ifndef CONTROLZ_ROUNDTRIP_TEST_SUPPORT_HPP
#define CONTROLZ_ROUNDTRIP_TEST_SUPPORT_HPP

#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <string>

namespace ControlZTests {

inline void require(bool condition, const char* expression, const char* file, int line) {
    if (!condition) {
        std::cerr << file << ':' << line << " requirement failed: " << expression << '\n';
        throw std::runtime_error(expression);
    }
}

inline std::filesystem::path temp_path(const char* name) {
    return std::filesystem::temp_directory_path() / (std::string("controlz-") + name);
}

inline void remove_if_present(const std::filesystem::path& path) {
    std::error_code error;
    std::filesystem::remove(path, error);
}

} // namespace ControlZTests

#define REQUIRE(condition) ::ControlZTests::require((condition), #condition, __FILE__, __LINE__)

#endif
