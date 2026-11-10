#ifndef CONTROLZ_COMMON_HPP
#define CONTROLZ_COMMON_HPP

#include <cstdint>
#include <random>
#include <concepts>
#include <bit>
#include <algorithm>

namespace ControlZ {


/// @brief Alias for a `UserID` (16bit unsigned integer)
using UserID = std::uint16_t;
/// @brief Alias for a `ChatID` (32bit unsigned integer)
using ChatID = std::uint32_t;
/// @brief Alias for a custom timestamp in seconds (32bit unsigned integer)
using Timestamp = std::uint32_t;
/// @brief Alias for an `AttachmentID` (32bit unsigned integer)
using AttachmentID = std::uint32_t;
/// @brief Alias for a `SequenceID` (16bit unsigned integer)
using SequenceID = std::uint16_t;
/// @brief Alias for a `SessionToken` (16byte `char` array)
using SessionToken = std::array<char, 16>;

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
        name ## _enum value = name::name ## _enum::def; \
        public: \
        name() noexcept = default; \
        ~name() noexcept = default; \
        constexpr name(name ## _enum x) noexcept : value(x) /* construct out of the internal enum */ {} \
        constexpr name(backing x) noexcept : value(static_cast<name ## _enum>(x)) /* construct out of the backing type */ {} \
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
        inline constexpr name operator~() const noexcept /* unary ~ */ { return name(~value); } \
        explicit inline constexpr operator bool() const noexcept /* allow in if statements */ { \
            return value != name::name ## _enum::zero; \
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
    Hexcl /* written as H! */, // Online help file or pertext database
    Hminmin /* written as H-- */, // C-- header file
    H, // C header file
    HA, // Archive
    HACK, // Hack language source
    HAR, // HTTP Archive format
    //HDF and HDF5
    HDI, // Hard Disk Image file
    HDMP, // Heap Dump file
    HEIC, // HEIF raster image
    HEIF, // HEIF raster image
    HH, // C++ header file
    HPP, // C++ header file
    HTA, // HTML Application
    HTM, // HTML source
    HTML, // HTML source
    HXX, // C++ header file

    // I ---------------------------------------
    ICL, // Windows Icon Library
    ICNS, // Macintosh icons
    ICO, // Icon file
    ICS, // iCalendar format
    IMG, // Disk Image
    INFO, // Textinfo document
    INI, // INI Configuration file
    IO, // Archive
    IOS, // GameMaker project code
    ISO, // Optical disk media ISO-standardized filesystem

    // J ---------------------------------------
    JL, // Julia language source
    JAR, // Java archive
    JAV, // Java language source
    JAVA, // Java language source
    JPE, // Same as JPEG
    JPEG, // Joint Photographic Experts Group
    JPG, // Same as JPEG
    JS, // JavaScript language source
    JSON, // JSON source

    // K ---------------------------------------
    KEY, // Keynote presentation
    KO, // Linux kernel module format system file
    KRA, // Krita project file
    KRABER, // Kraber language source
    KSH, // Kornshell source
    KT, // Kotlin language source

    // L ---------------------------------------
    LATEX, // LaTeX source
    LDB, // Microsoft Access Database
    LISP, // Lisp language source
    LL, // LLVM assembly
    LM, // Language Model file
    LNK, // Windows local file shortcut
    LOGICX, // Logic Pro project file
    LRC, // Lyrics file
    LUA, // Lua language source
    LZ, // Archive

    // M ---------------------------------------
    M, // Objective-C language source
    M4A, // MPEG-4 Part 14 audio
    M4R, // Same as M4A
    MAT, // Matlab language source
    MBOX, // Email mailbox archive
    MCADDON, // Minecraft addon
    MCMETA, // Minecraft resource pack configuration file
    MCFUNCTION, // Minecraft datapack command script
    MCPACK, // Minecraft resource pack
    MCPROJECT, // Minecraft Bedrock Editor file
    MCSTRUCTURE, // Minecraft structure
    MCTEMPLATE, // Mincraft world template
    MCWORLD, // Minecraft world
    MD, // Markdown source
    MDB, // Microsoft Access Database
    MDF, // Microsoft SQL Server Master Data File
    MDL, // Model
    MDMP, // Microsoft Minidump file
    MEX, // Matlab executable command
    MHT, // MIME encapsulation of aggregate HTML files
    MID, // MIDI file
    MKA, // Matroska Audio
    MKV, // Matroska Video
    MLX, // Matlab live script file
    MM, // Objective-C++ language source
    MML, // MathML source
    MOBI, // eBook
    MOD, // Modula language source
    MODULES, // GTK+ module file
    MOV, // Movie video file
    MP2, // MPEG-2 audio file
    MP3, // MPEG-3 audio file
    MP4, // MPEG-4 video file
    MPA, // MPEG audio stream
    MPEG, // MPEG media
    MPG, // MPEG media
    MSC, // Management Saved Console
    MSI, // Windows Installer package
    MSO, // Microsoft Outlook metadata file
    MSSTYLES, // Windows visual style file
    MSU, // Microsoft Update package

    // N ---------------------------------------
    NC, // Binary data
    NFO, // Info file
    NIM, // Nim language source
    NRO, // Nintendo Switch executable file

    // O ---------------------------------------
    O, // Object file
    OBJ, // Object code
    ODB, // OpenDocument Database
    ODF, // OpenDocument Formula
    ODG, // OpenDocument Graphics
    ODP, // OpenDocument Presentation
    ODS, // OpenDocument Spreadsheet
    ODT, // OpenDocument Text
    OGA, // Ogg audio
    OGG, // Vorbis audio
    OGV, // Ogg video
    OPUS, // Opus audio
    ORG, // Emacs ORG mode
    OSB, // Osu! storyboard
    OSK, // Osu! skin
    OSR, // Osu! replay
    OST, // Offline Storage Table
    OSU, // Osu! beatmap info
    OSZ, // Osu! beatmap
    OTF, // Open-Type Font or OpenDocument Formula Template
    OTG, // OpenDocument Graphics Template
    OTP, // OpenDocument Presentation Template
    OTS, // OpenDocument Spreadsheet Template
    OTT, // OpenDocument Text Template
    OXT, // OpenOffice or LibreOffice Extension

    // P ---------------------------------------
    
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
        value = std::byteswap(value);
}

/// @brief Return a value with the converted endianness of another value
///        of an integral type if the host is little endian
/// @tparam Type The type of the value
/// @param value The value to convert
template <std::integral Type>
inline constexpr Type network_byte_order_copy(Type value) noexcept {
    if constexpr (std::endian::native == std::endian::little)
        return std::byteswap(value);
    else
        return value; // Return something anyways
}

/// @brief Convert in-place the endianness of an enum if the host is
///        little endian
/// @tparam Type The type of the enum
/// @param value The value to convert
template <typename Type>
requires (std::is_enum_v<Type>)
inline constexpr void network_byte_order(Type& value) noexcept {
    if constexpr (std::endian::native == std::endian::little)
        value = static_cast<Type>(std::byteswap(std::to_underlying(value)));
}

/// @brief Return a value with the converted endianness of another value
///        of an enum type if the host is little endian
/// @tparam Type The type of the enum
/// @param value The value to convert
template <typename Type>
requires (std::is_enum_v<Type>)
inline constexpr Type network_byte_order_copy(Type value) noexcept {
    if constexpr (std::endian::native == std::endian::little)
        return static_cast<Type>(std::byteswap(std::to_underlying(value)));
    else
        return value;
}

/// @brief Convert in-place the endianness of an `std::string`'s bytes
///        if the host is little endian. This can be used on any serialized payload
/// @param str The string to convert
template <>
inline constexpr void network_byte_order(std::string& str) noexcept {
    if constexpr (std::endian::native != std::endian::little) return;
    std::reverse(str.begin(), str.end()); // This is actual endianness conversion
}

/// @brief Return a value with the converted endianness of the bytes another
///        value of type `std::string` if the host is little endian.
///        This can be used on any serialized payload
/// @param str The string to convert
template <>
inline constexpr std::string network_byte_order_copy(const std::string& str) noexcept {
    if constexpr (std::endian::native != std::endian::little) return str;
    std::string result = str; // Copy it
    std::reverse(result.begin(), result.end());
    return result;
}

/// @brief Convert in-place the endianness of the elements of an `std::vector`
/// @tparam Item The type of the elements in the vector
/// @param vec The vector
template <typename Item>
inline constexpr void network_byte_order(std::vector<Item>& vec) noexcept {
    if constexpr (std::endian::native != std::endian::little) return;
    for (auto& x : vec) network_byte_order(x);
}

/// @brief Return a copy of the passed-in `std::vector` with the endianness of its
///        elements converted
/// @tparam Item The type of the elements in the vector
/// @param vec The vector
template <typename Item>
inline constexpr std::vector<Item> network_byte_order_copy(const std::vector<Item>& vec) noexcept {
    if constexpr (std::endian::native != std::endian::little) return vec;
    std::vector<Item> self = vec;
    network_byte_order(self);
    return self;
}

template <std::integral Item>
inline constexpr void unique_sorted_vec(std::vector<Item>& vec) {
    std::sort(vec.begin(), vec.end()); // Sort elements
    auto end = std::unique(vec.begin(), vec.end()); // Get the new vector end
    vec.erase(end, vec.end()); // Erase all elements past it
}


}

#endif // CONTROLZ_COMMON_HPP
