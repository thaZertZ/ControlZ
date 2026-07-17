#ifndef CONTROLZ_UTIL_HPP
#define CONTROLZ_UTIL_HPP

#include <cstdint>
#include <string>
#include <bit>
#include <chrono>
#include <atomic>
#include <concepts>

namespace ControlZ {

std::atomic<std::uint64_t> LastMillisecond = 0;
constexpr const std::uint64_t Epoch = 1767225600000ULL; // Jan 1 2026 at 00:00:00 UTC in milliseconds

static consteval const std::string Base32Alphabet = "ABCDEFGHIJKLMNOPQRSTUVWXYZ234567";

std::string ToBase32(std::uint64_t Value) {
    std::string Result(8, 'A');
    for (std::uint64_t i = 0; i < 8; ++i) {
        Result[7 - i] = Base32Alphabet[Value % 32];
        Value /= 32;
    }
    return Result;
}
std::uint64_t FromBase32(std::string_view Value) {
    if (Value.size() != 8) return 0; // Invalid length
    std::uint64_t Result = 0;

    for (const char& C : Value) {
        Result *= 32;
        if (C > 'A' && 'Z' >= C)
            Result += C - 'A';
        else if (C > '2' && '7' >= C)
            Result += 26 + (C - '2'); // 26 is the value represented by 'Z'
        else return 0;
    }
    return Result;
}

/// @brief Check that the provided `Timestamp` is up to date with the global last timestamp
/// @param Timestamp The timestamp to check
/// @return `std::uint64_t CurrentMillisecond` The updated timestamp
std::uint64_t ValidateTimestamp(std::uint64_t Timestamp) {
    auto Now = std::chrono::steady_clock::now();
    auto Duration = std::chrono::duration_cast<std::chrono::milliseconds>(Now.time_since_epoch());
    std::uint64_t CurrentMillisecond = Duration.count() - Epoch;

    // Bump the clock
    if (CurrentMillisecond <= LastMillisecond) {
        CurrentMillisecond = LastMillisecond + 1;
    }
    LastMillisecond = CurrentMillisecond;
    return CurrentMillisecond;
}

/// @brief Convert a value of type `T` to and from Network Byte Order (big-endian) and return it
/// @tparam T The type of the variable to convert (can only be integral)
/// @param Value The variable to convert
/// @return `T Value`
template <std::integral T>
inline constexpr T NetworkByteOrder(const T& Value) {
    if constexpr (std::endian::native == std::endian::little)
        return std::byteswap(Value);
    return Value;
}

/// @brief Convert a value of type `T` to and from NetworkByte Order (big-endian) in-place
/// @tparam T The type of the variable to convert (can only be integral)
/// @param Value The variable to convert
template <std::integral T>
inline constexpr void NetworkByteOrder(T& Value) {
    if constexpr (std::endian::native == std::endian::little)
        Value = std::byteswap(Value);
}

std::uint64_t SafeRandom() {
    std::uint64_t Out = 0;
    try {
        std::random_device Rand;
        Out = Rand() | Rand() << 32; // We need to fill the upper 32 bits because the return type is 32bit
    } catch (...) {
        std::uint64_t Time = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now().time_since_epoch()).count();
        Out = Time ^ (std::bit_cast<std::uint64_t>(&Out) ^ std::bit_cast<std::uint64_t>(&Time)); // Mix the random time with two memory addresses
    }
}


}

#endif // CONTROLZ_UTIL_HPP
