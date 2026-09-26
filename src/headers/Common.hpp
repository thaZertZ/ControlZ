#ifndef CONTROLZ_COMMON_HPP
#define CONTROLZ_COMMON_HPP

#include <cstdint>
#include <random>
#include <concepts>
#include <bit>

namespace ControlZ {


/// @brief Alias for a UserID (16bit unsigned integer)
using UserID = std::uint16_t;
/// @brief Alias for a custom Timestamp in seconds (32bit unsigned integer)
using Timestamp = std::uint32_t;
/// @brief Alias for an AttachmentID (16bit unsigned integer)
using AttachmentID = std::uint16_t;

/// @brief Create a scoped enum type with comparison operator overloads and utility methods
/// @param name The type name
/// @param backing The backing type of the enum
/// @param def The default value for the enum (used by the default constructor)
/// @param zero The zero value (used when converting to `bool`)
/// @param __VA_ARGS__... The enum variants separated by commas and with values optionally
///                       assigned to them
#define CONTROLZ_MAKE_SCOPED_ENUM(name, backing, def, zero, ...) \
    class name { \
        public: \
        enum name ## _enum : backing /* internal scoped enum */ { \
            __VA_ARGS__ \
        }; \
        private: \
        name ## _enum value = ::ControlZ::name::def; \
        public: \
        name() noexcept = default; \
        ~name() noexcept = default; \
        constexpr name(name ## _enum x) noexcept : value(x) /* construct out of the internal enum */ {} \
        inline constexpr name operator|(name other) const noexcept /* OR two values */ { \
            return name(static_cast<name ## _enum>(value | other.value)); \
        } \
        inline constexpr name operator|(name ## _enum other) const noexcept { \
            return name(static_cast<name ## _enum>(value | other)); \
        } \
        inline constexpr name& operator|=(name other) noexcept { \
            return *this = *this | other; \
        } \
        inline constexpr name& operator|=(name ## _enum other) noexcept { \
            return *this = *this | other; \
        } \
        inline constexpr name operator&(name other) const noexcept /* AND two values */ { \
            return name(static_cast<name ## _enum>(value & other.value)); \
        } \
        inline constexpr name operator&(name ## _enum other) const noexcept { \
            return name(static_cast<name ## _enum>(value & other)); \
        } \
        inline constexpr name& operator&=(name other) noexcept { \
            return *this = *this & other; \
        } \
        inline constexpr name& operator&=(name ## _enum other) noexcept { \
            return *this = *this & other; \
        } \
        inline constexpr bool operator==(name ## _enum other) const noexcept { \
            return value == other; \
        } \
        inline constexpr bool operator!=(name ## _enum other) const noexcept { \
            return !(*this == other); \
        } \
        inline constexpr operator name ## _enum () const noexcept /* allow in switch statements */ { \
            return value; \
        } \
        inline constexpr backing operator+() const noexcept /* unary + */ { return value; } \
        explicit inline constexpr operator bool() const noexcept /* allow in if statements */ { \
            return value != ::ControlZ::name::zero; \
        } \
    };

/// @brief A 16bit enum used for representing any major file extension without storing a string
enum class FileExtension : std::uint16_t {
    Other, // The extension is not in the list
    None, // No file extension

    // Source: https://en.wikipedia.org/wiki/Lists_of_filename_extensions

    // A ----------------------------------------
    AHK, // AutoHotkey script
    AI, // Adobe Illustrator
    AIFF, // Audio Interchange File Format
    ANI, // Animated Windows mouse cursor
    APK, // Android Application Package
    //ARC,
    ASM, // Assembly language source
    AU, // Audio Unit
    AVI, // Audio Video Interleave
    AVIF, // AV1 image format

    // B ----------------------------------------
    B, // BASIC or B language source
    BAK, // Backup file
    BAS, // BASIC language source
    BAT, // Windows Batch script
    BIN, // Binary file
    BLEND, // Blender project file
    BSON, // Binary-serialized JSON
    BSP, // Binary space partitioning tree file

    // C ----------------------------------------
    C, // C language source
    Cplusplus /* written as C++ */, // C++ language source
    CPP, // C++ language source
    CAB, // Cabinet archive file
    CBL, // COBOL language source
    CC, // C++ language source
    CER, // Security certificate
    //CERT
    CLAP, // Clever Audio Plugin
    CLASS, // Java class file
    CMD, // Command prompt Windows Batch script
    COB, // COBOL language source
    COM, // DOS program
    CONFIG, // Configuration file
    CPL, // Windows Control Panel file
    CR2, // Raw image format
    CR3, // Raw image format
    CRT, // Security certificate
    CS, // C# language source
    CSPROJ, // C# project file
    CSS, // Cascading Style Sheet
    CSV, // Comma Separated Values
    CUR, // Non-animated Windows cursor
    CXX, // C++ language source

    // D ----------------------------------------
    D, // D language source or directory containing config files on Unix systems
    DART, // Dart language source
    DAT, // Generic data file (database, binary, ASCII...)
    DB, // Database file
    DBG, // Microsoft C/C++ debug symbols
    DEB, // Debian application package
    DLL, // Dynamically Linked Library
    DMP, // Memory dump file
    DOC, // Microsoft Word document
    DOCX, // Microsoft Word XML document

    // E ----------------------------------------
    E, // E language source
    EFI, // Extensible Firmware Interface
    EL, // Emacs Lisp
    ELC, // Emacs Lisp Compiled
    ELF, // Executable and Linkable Format
    EMAIL, // Microsoft Outlook Express email message
    EML, // RFC 5322 conforming email
    ERL, // Erlang language source
    EX, // Elixir language source
    EXE, // PE executable file
    EXS, // Elixir language source

    // F ----------------------------------------
    F, // Forth or Fortran language source
    F03, // Fortran language source
    F08, // Fortran language source
    F18, // Fortran language source
    F4, // Fortran language source
    F77, // Fortran language source
    F90, // Fortran language source
    F95, // Fortran language source
    FB, // Forth block file
    FLAC, // Audio codec and format
    FLP, // FL Studio project file
    FOR, // Fortran language source
    FRM, // MySQL database metadata
    FS, // F# language source
    FTH, // Forth language source

    // G ----------------------------------------
    GDSCRIPT, // Godot Script
    GGB, // GeoGebra file
    GIF, // Graphics Interchange Format
    GMD, // Geometry Dash level
    //GMEZ
    GML, // GameMaker script file
    GO, // Go language source
    GODOT, // Godot project file
    GZ, // Gzip compressed data
    GSLIDES, // Google Slides presentation

    // H ---------------------------------------
    
};


/// @brief Generate an unsigned 64bit random number safely
std::uint64_t random_number() noexcept {
    static thread_local std::mt19937 twister(std::random_device{}());
    std::uniform_int_distribution<std::uint64_t> dist(0, ~0ULL);
    return dist(twister);
}

/// @brief Helper concept to check if the types of a variadic type
///        pack are the same as a specified type
/// @tparam First The type to check for
/// @tparam ...Others The variadic type pack
template <typename First, typename... Others>
concept AllSameAs = (std::same_as<First, Others> && ...);

// Forward declarations

template <typename Type>
inline constexpr void network_byte_order(Type&) noexcept;

template <typename Type>
inline constexpr Type network_byte_order_copy(const Type&) noexcept;

/// @brief Convert in-place the endianness of a value of an integral
///        type if the host is little endian
/// @tparam Type The type of the value
/// @param value The value to convert
template <std::integral Type>
inline constexpr void network_byte_order(Type& value) noexcept {
    if constexpr (std::endian::native == std::endian::little)
        value = std::byteswap(value); // Maybe `std::move()` this?
}

/// @brief Return a value with the converted endianness of another value
///        of an integral type if the host is little endian
/// @tparam Type The type of the value
/// @param value The value to convert
template <std::integral Type>
inline constexpr Type network_byte_order_copy(const Type& value) noexcept {
    if constexpr (std::endian::native == std::endian::little)
        return std::byteswap(value);
    else
        return value; // Return something anyways
}

/// @brief Convert in-place the endianness of an `std::string`'s bytes
///        if the host is little endian. This can be used on any serialized payload
/// @param str The string to convert
template <>
inline constexpr void network_byte_order(std::string& str) noexcept {
    if constexpr (std::endian::native != std::endian::little) return;
    std::size_t size = str.size();
    for (std::size_t i = 0; i < size; i += 2) {
        if (i == size - 1) break; // We reached an odd place and we can't swap

        char& bucket1 = str[i];
        char& bucket2 = str[i + 1];
        // Old but gold trick
        bucket1 ^= bucket2;
        bucket2 ^= bucket1;
        bucket1 ^= bucket2;
    }
}

/// @brief Return a value with the converted endianness of the bytes another
///        value of type `std::string` if the host is little endian.
///        This can be used on any serialized payload
/// @param str The string to convert
template <>
inline constexpr std::string network_byte_order_copy(const std::string& str) noexcept {
    if constexpr (std::endian::native != std::endian::little) return str;

    // Same exact logic as before
    std::string result = str;
    std::size_t size = result.size();
    for (std::size_t i = 0; i < size; i += 2) {
        if (i == size - 1) break;

        char& bucket1 = result[i];
        char& bucket2 = result[i + 1];
        bucket1 ^= bucket2;
        bucket2 ^= bucket1;
        bucket1 ^= bucket2;
    }

    return result;
}


}

#endif // CONTROLZ_COMMON_HPP
