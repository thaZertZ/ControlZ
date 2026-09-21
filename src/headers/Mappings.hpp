#ifndef CONTROLZ_MAPPINGS_HPP
#define CONTROLZ_MAPPINGS_HPP

#include "Common.hpp"
#include <filesystem>
#include <fstream>
#include <string>
#include <cstring>
#include <expected>

namespace std {

namespace fs = ::std::filesystem;

} // namespace std

namespace ControlZ {


/// @brief Alias for an array of 32 unsigned 64bit integers, used as the master table of a mapping file
using MapMasterTable = std::uint64_t[32];
/// @brief Alias for an array of 256 unsigned 64bit integers, used as a page table of a mapping file
using MapPageTable = std::uint64_t[256];

/// @brief A scoped enum returned when verifying the state of a mapping file header
CONTROLZ_MAKE_SCOPED_ENUM (
    MapHeaderVerifyError, // Type name
    std::uint8_t, // Backing type
    OK, // Default value
    OK, // Zero value

    // Enum values
    OK             = 0,
    InvalidMagic   = 1 << 0,
    InvalidVersion = 1 << 1,
)

/// @brief A struct representing the configuration flags of a mapping file header
struct MapHeaderConfig {
    /// @brief The format version
    std::uint8_t version = 0;
    /// @brief Indicates whether the whole file is full of mappings
    bool full = false;
    /// @brief Holds the index of the first slot in the master table which points to a partially-empty page table
    std::uint8_t last_slot = 0;

    /// @brief Serialize this configuration struct into a valid byte value used in the header
    inline constexpr std::uint8_t to_config() noexcept {
        return (version & 0b00000011) | ((full ? 1 : 0) << 2) | (last_slot << 3);
    }

    /// @brief Unpack a header byte containing configuration into the individual fields
    /// @param config The header byte to unpack
    static inline constexpr MapHeaderConfig from_config(std::uint8_t config) noexcept {
        return {
            .version   = (std::uint8_t) (config       & 0b00000011),
            .full      = (std::uint8_t)((config >> 2) & 1) != 0, // Is the cast needed here?
            .last_slot = (std::uint8_t) (config >> 3)
        };
    }
};

#pragma pack(push, 1)

/// @brief A struct representing the header of a mapping file
struct MapHeader {
    /// @brief Three magic bytes to identify the format
    const char magic[3] = {'M', 'A', 'P'};

    /// @brief Configuration flags:
    /// ```txt
    /// 7 6 5 4 3 2 1 0
    /// ---------------
    /// s s s s s u v v
    /// 
    /// v  :  Version number
    /// u  :  Full flag
    /// s  :  Last available slot of master table
    /// ```
    std::uint8_t config = 0;

    /// @brief Check for illegal states in this header instance
    /// @return A scoped bitmask enum with the status of this header
    inline constexpr MapHeaderVerifyError verify() const noexcept {
        MapHeaderVerifyError errors;

        if (magic[0] != 'M' || magic[1] != 'A' || magic[2] != 'P')
            errors |= MapHeaderVerifyError::InvalidMagic;
        if ((config & 0b00000011) != 0)
            errors |= MapHeaderVerifyError::InvalidVersion;

        return errors;
    }
};

/// @brief A struct representing the header of each page inside a mapping file
struct MapPageHeader {
    /// @brief A single magic byte to identify the page
    const char magic = 'T';
    /// @brief The last slot available in the page table
    std::uint8_t last_slot = 0;

    /// @brief Check for illegal states in this header instance
    /// @return A scoped bitmask enum with the status of this header
    inline constexpr MapHeaderVerifyError verify() const noexcept {
        if (magic != 'T') return MapHeaderVerifyError::InvalidMagic;
        return MapHeaderVerifyError::OK;
    }
};

#pragma pack(pop)

/// @brief A struct representing a mapping between an `AttachmentID`
///        and a string containing its original filename
struct AttachmentMapping {
    /// @brief The ID of the mapped attachment
    AttachmentID id = 0;
    /// @brief The string holding the original filename and extension
    std::string mapping;

    /// @brief Align the contents of the string to 2 bytes, occasionally
    ///        adding a null byte at the end of the buffer
    inline constexpr void align() noexcept {
        std::size_t size = mapping.size();
        if (size % 2 == 0) return;
        mapping += '\0';
        mapping.shrink_to_fit(); // Hopefully does nothing
    }

    /// @brief Return the total size of a buffer received by calling `serialize()`
    inline constexpr std::size_t size() const noexcept {
        return sizeof(id) + mapping.size();
    }

    /// @brief Serialize this mapping into a string of bytes. This does not take
    ///        into account alignment, so remember to call `align()` before this
    inline constexpr std::string serialize() const noexcept {
        std::string result(sizeof(id) + mapping.size(), '\0');
        std::memcpy(result.data(), &id, sizeof(id));
        std::memcpy(result.data() + sizeof(id), mapping.data(), mapping.size());
        return result;
    }

    /// @brief Construct a mapping object from a string of bytes. This doesn't check for
    ///        the provided buffer actually holding enough bytes to create a valid object
    /// @param data The string of bytes
    static inline constexpr AttachmentMapping from_bytes(const std::string& data) noexcept {
        AttachmentMapping result;
        std::memcpy(&result.id, data.data(), sizeof(result.id));
        result.mapping.resize(data.size() - sizeof(result.id), '\0');
        std::memcpy(result.mapping.data(), data.data() + sizeof(result.id), result.mapping.size());
        return result;
    }
};

/// @brief A scoped enum returned when creating an empty mapping file
CONTROLZ_MAKE_SCOPED_ENUM (
    CreateMapFileError, // Type name
    std::uint8_t, // Backing type
    OK, // Default value
    OK, // Zero value

    // Enum values
    OK                = 0,
    FileAlreadyExists = 1 << 0,
    InvalidExtension  = 1 << 1,
    FileFatal         = 1 << 2,
    FileNonFatal      = 1 << 3,
    Exception         = 1 << 4
)

/// @brief Create an empty mapping file
/// @param path The path to the new file
/// @param force_overwrite If this is `true`, if a file exists with the provided name, it is overwritten
/// @return A scoped bitmask enum holding errors if they occurred
[[nodiscard]]
CreateMapFileError create_map_file(const std::fs::path& path, bool force_overwrite = false) noexcept {

    if (std::fs::exists(path) && !force_overwrite) return CreateMapFileError::FileAlreadyExists;
    if (path.extension() != ".map") return CreateMapFileError::InvalidExtension;

    std::ofstream file(path, std::ios::binary);
    if (!file) return CreateMapFileError::FileFatal;
    file.exceptions(std::ios::badbit | std::ios::failbit);

    try {

        file.seekp(0, std::ios::beg);
        MapHeader header;

        file.write(reinterpret_cast<char*>(&header), sizeof(header));
        MapMasterTable table = {0}; // All zero right?
        file.write(reinterpret_cast<char*>(table), sizeof(table));

    } catch (const std::ios::failure&) {

        if (file.bad())
            return CreateMapFileError::FileFatal;
        else
            return CreateMapFileError::FileNonFatal;

    } catch (...) {
        return CreateMapFileError::Exception;
    }

    return CreateMapFileError::OK;

}

/// @brief A scoped enum returned when appending to a mapping file
CONTROLZ_MAKE_SCOPED_ENUM (
    AppendMapFileError,
    std::uint16_t,
    OK,
    OK,

    // Enum values
    OK               = 0,
    MisalignedData   = 1 << 0,
    FileDoesNotExist = 1 << 1,
    InvalidMagic     = 1 << 2,
    InvalidVersion   = 1 << 3,
    InvalidExtension = 1 << 4,
    AlreadyFull      = 1 << 5,
    DataWouldNotFit  = 1 << 6,
    TruncatedData    = 1 << 7,
    NowFilled        = 1 << 8,
    FileFatal        = 1 << 9,
    FileNonFatal     = 1 << 10,
    Exception        = 1 << 11
)

/// @brief Helper concept for a generic type allowed to be serialized into a mapping file
/// @tparam Type The generic type
template <typename Type>
concept MappingType = requires (std::string str) {
    { std::declval<Type>().size() } -> std::same_as<std::size_t>; // Size of hypothetically serialized data
    { std::declval<Type>().serialize() } -> std::same_as<std::string>; // Serialize to bytes
    { Type::from_bytes(str) } -> std::same_as<Type>; // Constructor
};

/// @brief An internal helper for `append_map_entry`. This is not intended to be used alone
template <MappingType Mapping>
AppendMapFileError append_map_entry(std::fstream& file, MapHeader& header,
        MapHeaderConfig& config, const Mapping& mapping) {

    // Preliminary guards
    if (mapping.size() % 2 != 0) return AppendMapFileError::MisalignedData;
    if (config.full) return AppendMapFileError::AlreadyFull;

    AppendMapFileError errors;

    // Calculate at which offset in the table we need to read
    std::uint32_t offset = config.last_slot * sizeof(std::uint64_t);
    file.seekg(sizeof(header) + offset, std::ios::beg);

    // Then read the address of the last page
    std::uint64_t master_slot = 0;
    file.read(reinterpret_cast<char*>(&master_slot), sizeof(master_slot));

    bool need_new_page = master_slot == 0;
    if (need_new_page) {
        file.seekp(0, std::ios::end);

        master_slot = file.tellp(); // EOF
        file.seekp(sizeof(header) + offset, std::ios::beg);
        // Write the new address
        file.write(reinterpret_cast<char*>(&master_slot), sizeof(master_slot));

        file.seekp(0, std::ios::end);

        // Write a new page but don't update it because we merge into the main flow branch
        MapPageHeader page;
        file.write(reinterpret_cast<char*>(&page), sizeof(page));

        // Same for the page table
        MapPageTable table = {0};
        file.write(reinterpret_cast<char*>(&table), sizeof(table));
    }

    // Go to the page
    file.seekg(master_slot, std::ios::beg);
    MapPageHeader page;
    file.read(reinterpret_cast<char*>(&page), sizeof(page));

    file.seekg(0, std::ios::end);
    std::uint64_t address = file.tellg();

    std::uint16_t page_slot = need_new_page ? 0 : page.last_slot + 1;
    if (page_slot >= 256) return AppendMapFileError::DataWouldNotFit;

    page.last_slot = static_cast<std::uint8_t>(page_slot);
    file.seekp(master_slot, std::ios::beg);
    file.write(reinterpret_cast<char*>(&page), sizeof(page));

    std::uint64_t table_entry = master_slot + sizeof(page) + page_slot * sizeof(std::uint64_t);
    file.seekp(table_entry, std::ios::beg);
    file.write(reinterpret_cast<char*>(&address), sizeof(address));

    file.seekp(address, std::ios::beg);
    std::string data = mapping.serialize();
    file.write(data.data(), data.size());

    if (page_slot == 255) {
        ++config.last_slot;

        if (config.last_slot == 31) {
            config.full = true;
            errors |= AppendMapFileError::NowFilled;
        }

        header.config = config.to_config();
        file.seekp(0, std::ios::beg);
        file.write(reinterpret_cast<char*>(&header), sizeof(header));
    }

    return errors;
}


/// @brief Append a single mapping object to a mapping file file
/// @param path The path to the file
/// @param mapping The mapping object to append
/// @return A scoped bitmask enum holding errors if they occurred
template <MappingType Mapping>
[[nodiscard]]
AppendMapFileError append_map_file(const std::fs::path& path, const Mapping& mapping) noexcept {

    if (!std::fs::exists(path)) return AppendMapFileError::FileDoesNotExist;
    if (path.extension() != ".map") return AppendMapFileError::InvalidExtension;

    AppendMapFileError errors;

    std::fstream file(path, std::ios::in | std::ios::out | std::ios::binary);
    if (!file) return AppendMapFileError::FileFatal;
    file.exceptions(std::ios::badbit | std::ios::failbit);

    try {

        file.seekg(0, std::ios::beg);

        MapHeader header;
        file.read(reinterpret_cast<char*>(&header), sizeof(header));

        MapHeaderVerifyError status = header.verify();
        if (status & MapHeaderVerifyError::InvalidMagic) errors |= AppendMapFileError::InvalidMagic;
        if (status & MapHeaderVerifyError::InvalidVersion) errors |= AppendMapFileError::InvalidVersion;

        if (errors) return errors;

        MapHeaderConfig config = MapHeaderConfig::from_config(header.config);

        return append_map_entry(file, header, config, mapping);

    } catch (const std::ios::failure&) {

        if (file.bad())
            return AppendMapFileError::FileFatal;
        else
            return AppendMapFileError::FileNonFatal;

    } catch (...) {
        return AppendMapFileError::Exception;
    }

}

/// @brief Append multiple mapping objects to a mapping file file
/// @param path The path to the file
/// @param mappings The mapping objects to append
/// @return A scoped bitmask enum holding errors if they occurred
template <MappingType Mapping>
[[nodiscard]]
AppendMapFileError append_map_file(const std::fs::path& path, const std::vector<Mapping>& mappings) noexcept {

    AppendMapFileError errors;

    if (!std::fs::exists(path)) errors |= AppendMapFileError::FileDoesNotExist;
    if (path.extension() != ".map") errors |= AppendMapFileError::InvalidExtension;

    std::fstream file(path, std::ios::in | std::ios::out | std::ios::binary);
    if (!file) return AppendMapFileError::FileFatal;
    file.exceptions(std::ios::badbit | std::ios::failbit);

    try {

        // Read header and config
        MapHeader header;
        file.seekg(0, std::ios::beg);
        file.read(reinterpret_cast<char*>(&header), sizeof(header));

        MapHeaderVerifyError status = header.verify();
        if (status & MapHeaderVerifyError::InvalidMagic) errors |= AppendMapFileError::InvalidMagic;
        if (status & MapHeaderVerifyError::InvalidVersion) errors |= AppendMapFileError::InvalidVersion;

        if (errors) return errors;

        MapHeaderConfig config = MapHeaderConfig::from_config(header.config);

        AppendMapFileError errors;

        for (const Mapping& mapping : mappings) {
            errors |= append_map_entry(file, header, config, mapping);

            if (errors & AppendMapFileError::NowFilled || errors & AppendMapFileError::DataWouldNotFit)
                errors = AppendMapFileError::TruncatedData;
            if (errors) break; // Runs also if the above condition is true
        }

        return errors;

    } catch (const std::ios::failure&) {

        if (file.bad())
            return AppendMapFileError::FileFatal;
        else
            return AppendMapFileError::FileNonFatal;

    } catch (...) {
        return AppendMapFileError::Exception;
    }
}

/// @brief A scoped enum returned when deserializing a mapping file
CONTROLZ_MAKE_SCOPED_ENUM (
    DeserializeMapFileError, // Type name
    std::uint8_t, // Backing type
    OK, // Default value
    OK, // Zero value

    // Enum values
    OK               = 0,
    InvalidMagic     = 1 << 0,
    InvalidVersion   = 1 << 1,
    InvalidExtension = 1 << 2,
    FileDoesNotExist = 1 << 3,
    MisalignedData   = 1 << 4,
    FileFatal        = 1 << 5,
    FileNonFatal     = 1 << 6,
    Exception        = 1 << 7
)

/// @brief Deserialize the contents of a mapping file
/// @tparam Mapping The type of mapping being deserialized.
///                 If this is the wrong type, a lot of things could go wrong
/// @param path The path to the file
/// @param preallocate If set to `true`, the returned buffer is
///                    preallocated before deserialization. This
///                    is useful when dealing with very large files
/// @param start_mapping The first mapping to include into the returned buffer
/// @param max_mappings How many mappings to include in the returned buffer
/// @return A scoped bitmask enum holding errors if they occurred
template <MappingType Mapping>
[[nodiscard]]
std::expected<std::vector<Mapping>, DeserializeMapFileError> deserialize_map_file(
        const std::fs::path& path, bool preallocate = false, std::size_t start_mapping = 0,
        std::size_t max_mappings = ~0ULL) noexcept {
            
    if (!std::fs::exists(path))
        return std::unexpected<DeserializeMapFileError>(DeserializeMapFileError::FileDoesNotExist);
    if (path.extension() != ".map")
        return std::unexpected<DeserializeMapFileError>(DeserializeMapFileError::InvalidExtension);

    std::uint64_t filesize = std::fs::file_size(path);
    if (filesize % 2 != 0)
        return std::unexpected<DeserializeMapFileError>(DeserializeMapFileError::MisalignedData);

    std::ifstream file(path, std::ios::binary);
    if (!file) return std::unexpected<DeserializeMapFileError>(DeserializeMapFileError::FileFatal);
    file.exceptions(std::ios::badbit | std::ios::failbit);

    try {

        std::vector<Mapping> result;
        // We reserve and not resize so that we can use `push_back()` in every case, then calling
        // `shrink_to_fit()` at the end (master table * page table)
        if (preallocate) result.reserve(std::min<std::size_t>(32 * 256, max_mappings));

        MapHeader header;
        file.read(reinterpret_cast<char*>(&header), sizeof(header));

        DeserializeMapFileError errors;
        MapHeaderVerifyError status = header.verify();
        if (status & MapHeaderVerifyError::InvalidVersion)
            errors |= DeserializeMapFileError::InvalidVersion;
        if (status & MapHeaderVerifyError::InvalidMagic)
            errors |= DeserializeMapFileError::InvalidMagic;
        if (errors) return std::unexpected<DeserializeMapFileError>(errors);

        MapHeaderConfig config = MapHeaderConfig::from_config(header.config);

        // Same as before if we know it's full and we don't do it twice
        if (config.full && !preallocate) result.reserve(std::min<std::size_t>(32 * 256, max_mappings));

        MapMasterTable master_table = {0};
        file.read(reinterpret_cast<char*>(&master_table), sizeof(master_table));

        std::size_t mapping_index = 0;
        bool reached_limit = max_mappings == 0;

        for (std::uint8_t master_index = 0;
                master_index <= config.last_slot && master_table[master_index] != 0 && !reached_limit;
                ++master_index) {
            MapPageHeader page;
            file.seekg(master_table[master_index], std::ios::beg); // Go to the next page
            file.read(reinterpret_cast<char*>(&page), sizeof(page)); // Read config

            MapPageTable page_table = {0};
            file.read(reinterpret_cast<char*>(&page_table), sizeof(page_table));

            for (std::size_t page_index = 0; page_index <= page.last_slot && !reached_limit; ++page_index) {
                // Aliases (kinda)
                std::uint64_t current = page_table[page_index];
                std::uint64_t next = 0;

                if (page_index + 1 < 256)
                    next = page_table[page_index + 1];
                else if (master_index < config.last_slot && master_table[master_index + 1] != 0) {
                    // The next page's header begins immediately after this page's
                    // final payload, before the next page's first payload.
                    next = master_table[master_index + 1];
                }

                if (mapping_index++ < start_mapping) continue;

                if (result.size() >= max_mappings) {
                    reached_limit = true; // Set it so the outer loop also breaks
                    break;
                }

                file.seekg(current, std::ios::beg); // Go there

                // How many bytes we need to read
                std::uint32_t size = (next != 0) ? (next - current) : (filesize - current);

                std::string data(size, '\0'); // Fill it first
                file.read(data.data(), size);
                result.push_back(std::move(Mapping::from_bytes(data))); // And push

                reached_limit = result.size() == max_mappings;
            }
        }

        result.shrink_to_fit();
        return result;

    } catch (const std::ios::failure&) {

        if (file.bad())
            return std::unexpected<DeserializeMapFileError>(DeserializeMapFileError::FileFatal);
        else
            return std::unexpected<DeserializeMapFileError>(DeserializeMapFileError::FileNonFatal);

    } catch (...) {
        return std::unexpected<DeserializeMapFileError>(DeserializeMapFileError::Exception);
    }

}


} // namespace ControlZ

#endif // CONTROLZ_MAPPINGS_HPP
