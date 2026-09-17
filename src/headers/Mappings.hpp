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


using MapMasterTable = std::uint64_t[32];
using MapPageTable = std::uint64_t[256];

CONTROLZ_MAKE_SCOPED_ENUM (
    MapHeaderVerifyError, // Type name
    std::uint8_t, // Backing type
    OK, // Default value
    OK, // Zero value

    // Enum values
    OK = 0,
    InvalidMagic = 1 << 0,
    InvalidVersion = 1 << 1,
)

struct MapHeaderConfig {
    std::uint8_t version = 0;
    bool full = false;
    std::uint8_t last_slot = 0;

    inline constexpr std::uint8_t to_config() noexcept {
        return (version & 0b00000111) | (last_slot << 3);
    }

    static inline constexpr MapHeaderConfig from_config(std::uint8_t config) noexcept {
        return {
            .version = (std::uint8_t)(config & 0b00000011),
            .full = (std::uint8_t)((config >> 2) & 1) != 0,
            .last_slot = (std::uint8_t)(config >> 3)
        };
    }
};

struct MapHeader {
    static constexpr const char magic[3] = {'M', 'A', 'P'};
    std::uint8_t config = 0;

    inline constexpr MapHeaderVerifyError verify() const noexcept {
        MapHeaderVerifyError errors;

        if (magic[0] != 'M' || magic[1] != 'A' || magic[2] != 'P')
            errors |= MapHeaderVerifyError::InvalidMagic;
        if ((config & 0b00000011) != 0)
            errors |= MapHeaderVerifyError::InvalidVersion;

        return errors;
    }
};

struct MapPageHeader {
    const char magic = 'T';
    std::uint8_t last_slot = 0;

    inline constexpr MapHeaderVerifyError verify() const noexcept {
        if (magic != 'T') return MapHeaderVerifyError::InvalidMagic;
        return MapHeaderVerifyError::OK;
    }
};

struct AttachmentMapping {
    AttachmentID id = 0;
    std::string mapping; // Holds the filename and the extension too

    inline constexpr std::size_t size() const noexcept {
        return sizeof(id) + mapping.size();
    }

    inline constexpr std::string serialize() const noexcept {
        std::string result(sizeof(id) + mapping.size(), '\0');
        std::memcpy(result.data(), &id, sizeof(id));
        std::memcpy(result.data() + sizeof(id), mapping.data(), mapping.size());
        return result;
    }

    static inline constexpr AttachmentMapping from_bytes(const std::string& data) noexcept {
        AttachmentMapping result;
        std::memcpy(&result.id, data.data(), sizeof(result.id));
        result.mapping.resize(data.size() - sizeof(result.id), '\0');
        std::memcpy(result.mapping.data(), data.data() + sizeof(result.id), result.mapping.size());
        return result;
    }
};

CONTROLZ_MAKE_SCOPED_ENUM (
    CreateMapFileError, // Type name
    std::uint8_t, // Backing type
    OK, // Default value
    OK, // Zero value

    // Enum values
    OK                = 0,
    FileAlreadyExists = 1 << 0,
    FileFatal         = 1 << 1,
    FileNonFatal      = 1 << 2,
    Exception         = 1 << 3
)

[[nodiscard]]
CreateMapFileError create_map_file(const std::fs::path& path, bool force_overwrite) noexcept {

    if (std::fs::exists(path) && !force_overwrite)
        return CreateMapFileError::FileAlreadyExists;

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

CONTROLZ_MAKE_SCOPED_ENUM (
    AppendMapFileError,
    std::uint8_t,
    OK,
    OK,

    // Enum values
    OK               = 0,
    MisalignedData   = 1 << 0,
    FileDoesNotExist = 1 << 1,
    AlreadyFull      = 1 << 2,
    DataWouldNotFit  = 1 << 3,
    NowFilled        = 1 << 4,
    FileFatal        = 1 << 5,
    FileNonFatal     = 1 << 6,
    Exception        = 1 << 7,
)

template <typename Type>
concept MappingType = requires (std::string str) {
    { std::declval<Type>().size() } -> std::same_as<std::size_t>; // Bytesize
    { std::declval<Type>().serialize() } -> std::same_as<std::string>; // Serialize to bytes
    Type::from_bytes(str); // Constructor
};

template <MappingType Mapping>
[[nodiscard]]
AppendMapFileError append_map_file(const std::fs::path& path,
        const Mapping& mapping) noexcept {

    if (mapping.size() % 2 != 0) return AppendMapFileError::MisalignedData;

    if (!std::fs::exists(path)) return AppendMapFileError::FileDoesNotExist;

    AppendMapFileError errors;

    std::fstream file(path, std::ios::in | std::ios::out | std::ios::binary);
    if (!file) return AppendMapFileError::FileFatal;
    file.exceptions(std::ios::badbit | std::ios::failbit);

    try {

        file.seekg(0, std::ios::beg);

        MapHeader header;
        file.read(reinterpret_cast<char*>(&header), sizeof(header));
        MapHeaderConfig config = MapHeaderConfig::from_config(header.config);

        // We first need to check if the last slot is full
        if (config.full) return AppendMapFileError::AlreadyFull;

        std::uint32_t offset = config.last_slot * sizeof(std::uint64_t);
        file.seekg(offset, std::ios::cur);
        std::uint64_t master_slot = 0;
        file.read(reinterpret_cast<char*>(&master_slot), sizeof(master_slot));

        // We need to allocate a new page
        if (master_slot == 0) {
            file.seekp(0, std::ios::end); // Go to the end
            // Set the correct address for the slot
            master_slot = file.tellp();
            file.seekp(sizeof(header) + offset, std::ios::beg);
            // And write it back
            file.write(reinterpret_cast<char*>(&master_slot), sizeof(master_slot));
            file.seekp(0, std::ios::end); // Again

            // We don't increment `page.last_slot` yet because we converge into the main flow later
            MapPageHeader page;
            file.write(reinterpret_cast<char*>(&page), sizeof(page));
            // We also don't write to the table for the same reason
            MapPageTable table = {0};
            file.write(reinterpret_cast<char*>(&table), sizeof(table));
        }

        // Continue from where we left off
        file.seekg(master_slot, std::ios::beg);
        MapPageHeader page;
        file.read(reinterpret_cast<char*>(&page), sizeof(page));

        file.seekg(0, std::ios::end);
        std::uint64_t address = file.tellg(); // Get the address at the end of the file

        offset = (std::uint64_t)file.tellg() + page.last_slot * sizeof(std::uint64_t);

        ++page.last_slot; // Increment it for next time and write it back
        file.seekp(master_slot, std::ios::beg);
        file.write(reinterpret_cast<char*>(&page), sizeof(page));

        file.seekp(master_slot + offset, std::ios::beg);
        file.write(reinterpret_cast<char*>(&address), sizeof(address)); // Write it to the table

        file.seekp(address, std::ios::beg);
        std::string data = mapping.serialize();
        file.write(data.data(), data.size());

        // If we filled the whole table we need to change the header
        if (page.last_slot == 255) {
            ++config.last_slot;

            if (config.last_slot == 31) {
                config.full = true;
                errors |= AppendMapFileError::NowFilled;
            }

            header.config = config.to_config();
            file.seekp(0, std::ios::beg);
            file.write(reinterpret_cast<char*>(&header), sizeof(header));
        }

    } catch (const std::ios::failure&) {

        if (file.bad())
            return AppendMapFileError::FileFatal;
        else
            return AppendMapFileError::FileNonFatal;

    } catch (...) {
        return AppendMapFileError::Exception;
    }

    return AppendMapFileError::OK;
}

CONTROLZ_MAKE_SCOPED_ENUM (
    DeserializeMapFileError, // Type name
    std::uint8_t, // Backing type
    OK, // Default value
    OK, // Zero value

    // Enum values
    OK               = 0,
    InvalidMagic     = 1 << 0,
    InvalidVersion   = 1 << 1,
    FileDoesNotExist = 1 << 2,
    MisalignedData   = 1 << 3,
    FileFatal        = 1 << 4,
    FileNonFatal     = 1 << 5,
    Exception        = 1 << 6
)

template <MappingType Mapping>
[[nodiscard]]
std::expected<std::vector<Mapping>, DeserializeMapFileError> deserialize_map_file(
        const std::fs::path& path, bool preallocate = false, std::size_t start_mapping = 0,
        std::size_t max_mappings = ~0ULL) noexcept {
            
    if (!std::fs::exists(path))
        return std::unexpected<DeserializeMapFileError>(DeserializeMapFileError::FileDoesNotExist);

    std::uint64_t filesize = std::fs::file_size(path);
    if (filesize % 2 != 0)
        return std::unexpected<DeserializeMapFileError>(DeserializeMapFileError::MisalignedData);

    std::ifstream file(path, std::ios::binary);
    if (!file) return std::unexpected<DeserializeMapFileError>(DeserializeMapFileError::FileFatal);
    file.exceptions(std::ios::badbit | std::ios::failbit);

    try {

        std::vector<Mapping> result;
        // We reserve and not resize so that we can use `push_back()` in every case, then calling
        // `shrink_to_fit()` at the end
        if (preallocate) result.reserve(32 * 256); // Master table * page table

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
        if (config.full && !preallocate) result.reserve(32 * 256);

        MapMasterTable master_table = {0};
        file.read(reinterpret_cast<char*>(&master_table), sizeof(master_table));

        for (std::uint8_t master_index = 0; master_index <= config.last_slot; ++master_index) {
            MapPageHeader page;
            file.seekg(master_table[master_index], std::ios::beg); // Go to the next page
            file.read(reinterpret_cast<char*>(&page), sizeof(page)); // Read config

            MapPageTable page_table = {0};
            file.read(reinterpret_cast<char*>(&page_table), sizeof(page_table));

            for (std::uint8_t page_index = 0; page_index <= page.last_slot; ++page_index) {
                // Aliases
                std::uint64_t current = page_table[page_index];
                std::uint64_t next = page_table[page_index + 1];

                file.seekg(current, std::ios::beg); // Go there

                // How many bytes we need to read
                std::uint32_t size = (next != 0) ? (next - current) : (filesize - current);

                std::string data(size, '\0'); // Fill it first
                file.read(data.data(), size);
                result.push_back(std::move(Mapping::from_bytes(data))); // And push
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
