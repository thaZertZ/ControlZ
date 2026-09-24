#ifndef CONTROLZ_TIME_HPP
#define CONTROLZ_TIME_HPP

#include "Common.hpp"
#include <chrono>

namespace ControlZ {


/// @brief Return the value of the custom epoch with a UNIX timestamp in seconds
inline constexpr std::int64_t custom_epoch_seconds() noexcept {
    using namespace std::chrono_literals;

    const auto epoch = std::chrono::sys_days { 2027y / std::chrono::January / 1d };
    return std::chrono::duration_cast<std::chrono::seconds>(epoch.time_since_epoch()).count();
}

/// @brief Turn the current UNIX timestamp into the custom timestamp
inline Timestamp timestamp_now() noexcept {
    const auto now = std::chrono::system_clock::now();
    const auto epoch = std::chrono::sys_seconds { std::chrono::seconds { custom_epoch_seconds() } };
    const auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - epoch).count();

    return elapsed > 0 ? static_cast<Timestamp>(elapsed) : 0; // Clamp to 0 if too low
}

/// @brief Convert a UTC UNIX timestamp into a custom UTC timestamp
/// @param t The UNIX timestamp
inline constexpr Timestamp unix_to_custom(std::int64_t t) noexcept {
    const auto epoch = custom_epoch_seconds();
    return t > epoch ? static_cast<Timestamp>(t - epoch) : 0;
}

/// @brief Convert a custom UTC timestamp to a UNIX UTC timestamp
/// @param t The custom timestamp
inline constexpr std::int64_t custom_to_unix(Timestamp t) noexcept {
    return custom_epoch_seconds() + static_cast<std::int64_t>(t);
}

/// @brief Convert a UTC UNIX timestamp to a local time UNIX timestamp
/// @param t The timestamp to convert
inline std::int64_t utc_to_local(std::int64_t t) /* noexcept */ {
    const auto& db = std::chrono::get_tzdb();
    const auto* current = db.current_zone(); // Can we make this noexcept?

    const auto local = current->to_local(std::chrono::sys_seconds(std::chrono::seconds(t)));
    return std::chrono::duration_cast<std::chrono::seconds>(local.time_since_epoch()).count();
}

/// @brief Convert a local UNIX timestamp to a UTC UNIX timestamp
/// @param t The timestamp to convert
inline std::int64_t local_to_utc(std::int64_t t) {
    const auto& db = std::chrono::get_tzdb();
    const auto* current = db.current_zone();

    const auto utc = current->to_sys(std::chrono::local_time<std::chrono::seconds>(std::chrono::seconds(t)));
    return std::chrono::duration_cast<std::chrono::seconds>(utc.time_since_epoch()).count();
}


} // namespace ControlZ

#endif // CONTROLZ_TIME_HPP
