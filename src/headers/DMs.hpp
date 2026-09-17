#ifndef CONTROLZ_DMS_HPP
#define CONTROLZ_DMS_HPP

#include "Common.hpp"
#include <expected>
#include <optional>
#include <string>
#include <fstream>
#include <filesystem>

namespace std {

namespace fs = ::std::filesystem;

} // namespace std

namespace ControlZ {

/// @brief Derive a context string from a variadic number of strings by concatenating
///        them and separating them with a non-breaking space
/// @tparam ...Args A variadic number of `std::string`s
/// @param x The first string
/// @param ...args All of the others
/// @return The concatenated strings
template <typename... Args>
requires (AllSameAs<std::string, Args...>)
inline constexpr std::string derive_context_string(const std::string& x, const Args&... args) {
    if constexpr (sizeof...(args) == 0) {
        return x;
    } else {
        return x + (char)255 + derive_context_string(args...);
    }
}

/// @brief Scramble the bits of a given number
/// @param x The number to scramble
/// @return The scrambled number
inline constexpr std::uint64_t avalanche_scramble(std::uint64_t x) noexcept {
    x = (x ^ (x >> 30)) * 0xbf58476d1ce4e5b9ULL;
    x = (x ^ (x >> 27)) * 0x94d049bb133111ebULL;
    x = x ^ (x >> 31);
    return x;
}

/// @brief Derive an encryption key from a randomly-generated seed and a context string
/// @param seed The seed
/// @param context_str The context string
/// @return The generated key
inline constexpr std::uint64_t derive_key(std::uint64_t seed, const std::string& context_str) noexcept {
    for (char c : context_str) {
        seed ^= static_cast<std::uint64_t>(c);
        seed = avalanche_scramble(seed);
    }

    return avalanche_scramble(seed);
}

/// @brief Perform symmetric encryption to a data out-parameter with a key
/// @param data The data out-parameter (will be modified when this function returns)
/// @param key The encryption key used
inline constexpr void encrypt_decrypt(std::string& data, std::uint64_t key) noexcept {
    for (char& c : data) {
        key = avalanche_scramble(key);
        c ^= static_cast<char>(key >> 56);
    }
}

/// @brief A scoped enum used when verifying the contents of a `DMsHeader`
CONTROLZ_MAKE_SCOPED_ENUM (
    DMsHeaderVerifyError, // Type name
    std::uint8_t, // Backing type
    OK, // Default value
    OK, // Zero value

    // Enum values
    OK             = 0,
    InvalidMagic   = 1 << 0,
    InvalidVersion = 1 << 1,
    InvalidConfig  = 1 << 2
)

#pragma pack(push, 1)

/// @brief The file header of the DMs binary format
struct DMsHeader {
    /// @brief `"DM"` magic bytes
    const char magic[2] = {'D', 'M'};
    /// @brief Version number
    std::uint8_t version = 0;
    /// @brief Configuration flags:
    /// ```txt
    /// 7 6 5 4 3 2 1 0
    /// ---------------
    /// x x x x x x l k
    ///
    /// k  :  Zstd compression flag (not yet implemented)
    /// l  :  Linking type (0 = forward and 1 = backward)
    /// x  :  Reserved for future use
    /// ```
    std::uint8_t config = 0;
    /// @brief The number of nodes contained in this file
    std::uint16_t node_count = 0;
    /// @brief The absolute byte address of the last node
    std::uint64_t last_node_addr = 0;

    /// @brief Verify the contents of a header object
    /// @return A status bitmask
    inline constexpr DMsHeaderVerifyError verify() const noexcept {

        DMsHeaderVerifyError errors;

        if (magic[0] != 'D' || magic[1] != 'M') errors |= DMsHeaderVerifyError::InvalidMagic;
        if (version > 0) errors |= DMsHeaderVerifyError::InvalidVersion;

        if ((config & 0b11111100) != 0) errors |= DMsHeaderVerifyError::InvalidConfig;

        return errors;
    }
};

/// @brief A struct representing a node in a DMs file
struct DMsNode {
    /// @brief The seed used in the encryption process of the contained data
    std::uint64_t seed = 0;
    /// @brief The relative byte offset to the next node according to the
    ///        linking type specified in the file header (with `linking type == 1`
    ///        this becomes `prev_node_offset`)
    std::uint32_t next_node_offset = 0;
};

#pragma pack(pop)


/// @brief A scoped enum returned when creating a DMs file
CONTROLZ_MAKE_SCOPED_ENUM (
    CreateDMsFileError, // Type name
    std::uint8_t, // Backing type
    OK, // Default value
    OK, // Zero value

    // Enum values
    OK                = 0,
    InvalidVersion    = 1 << 0,
    InvalidExtension  = 1 << 1,
    FileAlreadyExists = 1 << 2,
    FileFatal         = 1 << 3,
    FileNonFatal      = 1 << 4,
    Exception         = 1 << 5
)

/// @brief Create a DMs file with no nodes inside it
/// @param path The path to the file
/// @param version The version to create the file with (defaults to `0` and is the
///                only valid value)
/// @param linking_type Set to `false` if the linking type if forward and `true`
///                     if it is backward
/// @param compress Set to `true` to flag the file as needing to be Zstd-compressed
/// @param force_overwrite Set to `true` if to overwrite an existing file with the
///                        provided name
/// @return An enum bitmask with one or more errors that occurred during the operation
[[nodiscard]]
CreateDMsFileError create_dms_file(const std::fs::path& path, std::uint8_t version = 0,
        bool linking_type = false, bool compress = false, bool force_overwrite = false) noexcept {

    CreateDMsFileError errors;

    if (version != 0) errors |= CreateDMsFileError::InvalidVersion;
    if (path.extension() != ".dm") errors |= CreateDMsFileError::InvalidExtension;

    DMsHeader header;
    header.node_count = 1;
    header.config = ((linking_type ? 1 : 0) << 1) | (compress ? 1 : 0);

    if (std::fs::exists(path) && !force_overwrite)
        errors |= CreateDMsFileError::FileAlreadyExists;

    if (errors != CreateDMsFileError::OK) return errors; // Return early rip -_-

    std::ofstream file(path, std::ios::binary);
    if (!file) return CreateDMsFileError::FileFatal; // Nice and easy
    file.exceptions(std::ios::badbit | std::ios::failbit);

    try {

        file.seekp(std::ios::beg);
        file.write(reinterpret_cast<char*>(&header), sizeof(header));

    } catch (const std::ios::failure&) {

        if (file.bad())
            return CreateDMsFileError::FileFatal;
        else
            return CreateDMsFileError::FileNonFatal;

    } catch (...) {
        return CreateDMsFileError::Exception;
    }

    return CreateDMsFileError::OK;
}


/// @brief A scoped enum returned when appending to a DMs file
CONTROLZ_MAKE_SCOPED_ENUM (
    AppendDMsFileError, // Type name
    std::uint16_t, // Backing type
    OK, // Default value
    OK, // Zero value

    // Enum values
    OK                  = 0,
    FileTooSmall        = 1 << 0,
    InvalidMagic        = 1 << 1,
    InvalidVersion      = 1 << 2,
    InvalidConfig       = 1 << 3,
    InvalidLastNodeAddr = 1 << 4,
    MisalignedData      = 1 << 5,
    FileDoesNotExist    = 1 << 6,
    FileFatal           = 1 << 7,
    FileNonFatal        = 1 << 8,
    Exception           = 1 << 9
)


/// @brief Append message data to a DMs file adding a new node
/// @param path The path to the file
/// @param data The data to write (it is copied to leave the caller with unencrypted data)
/// @param seed The seed with which to encrypt the data
/// @param context_str The context string used in the encryption process
/// @return An enum bitmask with one or more errors that occurred during the operation
[[nodiscard]]
AppendDMsFileError append_dms_file(const std::fs::path& path, std::string data,
        std::uint64_t seed, const std::string& context_str) noexcept {

    AppendDMsFileError errors;

    if (data.size() % 2 != 0) return AppendDMsFileError::MisalignedData; // We want padding of 2

    if (!std::fs::exists(path))
        return AppendDMsFileError::FileDoesNotExist;

    std::fstream file(path, std::ios::in | std::ios::out | std::ios::binary);
    if (!file) return AppendDMsFileError::FileFatal;
    file.exceptions(std::ios::badbit | std::ios::failbit);

    try {

        std::uint64_t filesize = std::fs::file_size(path);
        if (filesize <= sizeof(DMsHeader))
            return AppendDMsFileError::FileTooSmall;

        DMsHeader header;
        file.read(reinterpret_cast<char*>(&header), sizeof(header));

        DMsHeaderVerifyError status = header.verify();
        if (status & DMsHeaderVerifyError::InvalidMagic) errors |= AppendDMsFileError::InvalidMagic;
        if (status & DMsHeaderVerifyError::InvalidVersion) errors |= AppendDMsFileError::InvalidVersion;
        if (status & DMsHeaderVerifyError::InvalidConfig) errors |= AppendDMsFileError::InvalidConfig;

        if (errors) return errors;

        bool linking_type = ((header.config & 0b00000010) >> 1) != 0;
        if (!linking_type) { // Forward linked

            if (header.node_count == 1) { // Special case
                DMsNode first_node;
                file.read(reinterpret_cast<char*>(&first_node), sizeof(first_node));
                first_node.next_node_offset = filesize - sizeof(header);

                file.seekp(0, std::ios::beg);
            }

            // Update the header and write it back
            ++header.node_count;
            header.last_node_addr = filesize; // This case every time
            file.write(reinterpret_cast<char*>(&header), sizeof(header));
            file.seekp(0, std::ios::end);

            DMsNode node;
            node.seed = seed;
            file.write(reinterpret_cast<char*>(&node), sizeof(node)); // Write just the node

            std::uint64_t key = derive_key(seed, context_str);
            encrypt_decrypt(data, key);
            file.write(data.data(), data.size()); // Then the encrypted data

        } else { // Backward linked

            if (header.node_count == 1) { // Special case
                file.seekp(0, std::ios::beg);

                // Update header
                ++header.node_count;
                header.last_node_addr = filesize;
                file.write(reinterpret_cast<char*>(&header), sizeof(header));

                file.seekp(0, std::ios::end);
                DMsNode node;
                node.seed = seed;
                // Don't be fooled, it's a prev_node_offset when we link backward
                node.next_node_offset = filesize - sizeof(header);
                file.write(reinterpret_cast<char*>(&node), sizeof(node));
            } else {
                file.seekp(0, std::ios::beg);

                ++header.node_count;
                std::uint64_t ex_last_node_addr = header.last_node_addr; // Save it for linking later
                header.last_node_addr = filesize;
                file.write(reinterpret_cast<char*>(&header), sizeof(header));

                file.seekp(0, std::ios::beg);
                DMsNode node;
                node.seed = seed;
                node.next_node_offset = filesize - ex_last_node_addr; // Offset to previous node
                file.write(reinterpret_cast<char*>(&node), sizeof(node));
            }

            std::uint64_t key = derive_key(seed, context_str);
            encrypt_decrypt(data, key);
            file.write(data.data(), data.size());

        }

        return AppendDMsFileError::OK;

    } catch (const std::ios::failure&) {

        if (file.bad())
            return AppendDMsFileError::FileFatal;
        else
            return AppendDMsFileError::FileNonFatal;

    } catch (...) {
        return AppendDMsFileError::Exception;
    }
}


/// @brief A scoped enum returned when decrypting a DMs file
CONTROLZ_MAKE_SCOPED_ENUM (
    DecryptDMsFileError, // Type name
    std::uint16_t, // Backing type
    OK, // Default value
    OK, // Zero value

    // Enum values
    OK                  = 0,
    FileTooSmall        = 1 << 0,
    InvalidMagic        = 1 << 1,
    InvalidVersion      = 1 << 2,
    InvalidConfig       = 1 << 3,
    InvalidLastNodeAddr = 1 << 4,
    FileDoesNotExist    = 1 << 5,
    InvalidStartNode    = 1 << 6,
    ZeroMaxNodes        = 1 << 7,
    FileFatal           = 1 << 8,
    FileNonFatal        = 1 << 9,
    Exception           = 1 << 10
)


/// @brief Decrypt the contents of a DMs file
/// @param path The path to the file
/// @param context_str The context string used for the encryption process
/// @param start_node The node at which to start decrypting (defaults to `0`)
/// @param max_nodes The number of nodes to decrypt (defaults to `~0` and
///                  is clamped to the maximum valid value)
/// @return An enum bitmask with one or more errors that occurred during the operation
[[nodiscard]]
std::expected<std::vector<std::string>, DecryptDMsFileError> decrypt_dms_file(
        const std::fs::path& path, const std::string& context_str,
        const std::uint16_t start_node = 0, std::uint16_t max_nodes = ~0) noexcept {

    DecryptDMsFileError errors;

    if (max_nodes == 0) // Like what the hell
        return std::unexpected<DecryptDMsFileError>(DecryptDMsFileError::ZeroMaxNodes);

    if (!std::fs::exists(path)) errors |= DecryptDMsFileError::FileDoesNotExist;

    if (errors) return std::unexpected<DecryptDMsFileError>(errors);

    std::ifstream file(path, std::ios::binary);
    if (!file) return std::unexpected<DecryptDMsFileError>(DecryptDMsFileError::FileFatal);
    file.exceptions(std::ios::badbit | std::ios::failbit);

    try {

        std::uint64_t filesize = std::fs::file_size(path);
        if (filesize <= sizeof(DMsHeader))
            return std::unexpected<DecryptDMsFileError>(DecryptDMsFileError::FileTooSmall);

        file.seekg(0, std::ios::beg);

        DMsHeader header;
        file.read(reinterpret_cast<char*>(&header), sizeof(header));
        DMsHeaderVerifyError status = header.verify();
        if (status & DMsHeaderVerifyError::InvalidMagic) errors |= DecryptDMsFileError::InvalidMagic;
        if (status & DMsHeaderVerifyError::InvalidVersion) errors |= DecryptDMsFileError::InvalidVersion;
        if (status & DMsHeaderVerifyError::InvalidConfig) errors |= DecryptDMsFileError::InvalidConfig;

        if (errors) return std::unexpected<DecryptDMsFileError>(errors);

        // Check node ranges
        if (start_node >= header.node_count)
            return std::unexpected<DecryptDMsFileError>(DecryptDMsFileError::InvalidStartNode);
        if (start_node + max_nodes > header.node_count)
            max_nodes = header.node_count - start_node; // Clamp it

        bool linking_type = ((header.config & 0b00000010) >> 1) != 0;

        // Create it with the clamped size
        std::vector<std::string> result(max_nodes);

        if (!linking_type) { // Forward linking

            file.seekg(sizeof(DMsHeader), std::ios::beg);
            std::uint64_t data_size = 0;
            DMsNode node;
            std::uint64_t key = 0;

            // Placed oldest-first in the vector
            for (std::uint16_t i = 0; i < start_node + max_nodes; ++i) {
                file.read(reinterpret_cast<char*>(&node), sizeof(node));
                std::uint64_t data_start = file.tellg(); // Position right after reading the node

                // Pass iterations (we don't need to check for `max_nodes`)
                if (i < start_node) {
                    file.seekg(data_start + node.next_node_offset - sizeof(node));
                    continue;
                }

                if (node.next_node_offset == 0)
                    data_size = filesize - data_start; // From here to EOF
                else
                    data_size = node.next_node_offset - sizeof(node); // From here to next node

                std::string& bucket = result[i - start_node];
                bucket.resize(data_size, '\0');
                file.read(bucket.data(), data_size);
                key = derive_key(node.seed, context_str);
                encrypt_decrypt(bucket, key);

                // We might not need this since we already clamp `max_nodes`
                if (node.next_node_offset == 0) break;
            }

        } else { // Backward linking

            if (header.node_count == 1) { // Special case
                std::uint64_t data_size = filesize - sizeof(header) - sizeof(DMsNode);
                std::string& bucket = result[0];
                bucket.resize(data_size, '\0');

                DMsNode node; // Get the node for the seed
                file.seekg(sizeof(header), std::ios::beg);
                file.read(reinterpret_cast<char*>(&node), sizeof(node));

                file.read(bucket.data(), data_size);
                std::uint64_t key = derive_key(node.seed, context_str);
                encrypt_decrypt(bucket, key);

                return result; // The caller should use std::move() but maybe RVO does it already
            }

            file.seekg(0, std::ios::beg);
            std::uint64_t current_node_addr = 0;
            std::uint64_t data_size = 0;
            std::uint64_t prev_node_addr = filesize;
            DMsNode node;
            std::uint64_t key = 0;

            // Placed newest-first in the vector
            for (std::uint16_t i = 0; i < start_node + max_nodes; ++i) {
                current_node_addr = file.tellg();
                file.read(reinterpret_cast<char*>(&node), sizeof(node));

                // We do this after the read because we need the next offset
                if (i < start_node) { // Skip iteration
                    prev_node_addr = current_node_addr;
                    file.seekg(current_node_addr - node.next_node_offset, std::ios::beg);
                    continue;
                }

                data_size = prev_node_addr - file.tellg();

                std::string& bucket = result[i - start_node];
                bucket.resize(data_size, '\0');
                file.read(bucket.data(), data_size);
                key = derive_key(node.seed, context_str);
                encrypt_decrypt(bucket, key);

                // Still valid because the only node with an offset of 0 is the first one
                if (node.next_node_offset == 0) break;
                prev_node_addr = current_node_addr;
                // Go to next node
                file.seekg(current_node_addr - node.next_node_offset, std::ios::beg);
            }

        }

        return result;

    } catch (const std::ios::failure&) {

        if (file.bad())
            return std::unexpected<DecryptDMsFileError>(DecryptDMsFileError::FileFatal);
        else
            return std::unexpected<DecryptDMsFileError>(DecryptDMsFileError::FileNonFatal);

    } catch (...) {
            return std::unexpected<DecryptDMsFileError>(DecryptDMsFileError::Exception);
    }
}


/// @brief A scoped enum returned when converting a DMs file
CONTROLZ_MAKE_SCOPED_ENUM (
    ConvertDMsFileError, // Type name
    std::uint16_t, // Backing type
    OK, // Default value
    OK, // Zero value

    // Enum values
    OK                  = 0,
    FileTooSmall        = 1 << 0,
    InvalidMagic        = 1 << 1,
    InvalidVersion      = 1 << 2,
    InvalidConfig       = 1 << 3,
    InvalidLastNodeAddr = 1 << 4,
    FileDoesNotExist    = 1 << 5,
    TargetTypeIsCurrent = 1 << 6,
    FileFatal           = 1 << 7,
    FileNonFatal        = 1 << 8,
    Exception           = 1 << 9
)

/// @brief Convert a DMs file to a target linking type
/// @param path The path to the file
/// @param target_linking_type The linking type to convert the file to
/// @return An enum bitmask with one or more errors that occurred during the operation
[[nodiscard]]
ConvertDMsFileError convert_dms_file(const std::fs::path& path, bool target_linking_type) noexcept {

    ConvertDMsFileError errors;

    if (!std::fs::exists(path)) return ConvertDMsFileError::FileDoesNotExist;

    std::fstream file(path, std::ios::in | std::ios::out | std::ios::binary);
    if (!file) return ConvertDMsFileError::FileFatal;
    file.exceptions(std::ios::badbit | std::ios::failbit);

    try {

        std::uint64_t filesize = std::fs::file_size(path);
        if (filesize < sizeof(DMsHeader)) return ConvertDMsFileError::FileTooSmall;

        file.seekg(0, std::ios::beg);
        DMsHeader header;
        file.read(reinterpret_cast<char*>(&header), sizeof(header));

        DMsHeaderVerifyError status = header.verify();
        if (status & DMsHeaderVerifyError::InvalidMagic) errors |= ConvertDMsFileError::InvalidMagic;
        if (status & DMsHeaderVerifyError::InvalidVersion) errors |= ConvertDMsFileError::InvalidVersion;
        if (status & DMsHeaderVerifyError::InvalidConfig) errors |= ConvertDMsFileError::InvalidConfig;

        if (errors) return errors;

        bool linking_type = ((header.config & 0b00000010) >> 1) != 0;
        if (linking_type == target_linking_type) return ConvertDMsFileError::TargetTypeIsCurrent;

        std::vector<std::uint64_t> node_addrs;
        node_addrs.reserve(header.node_count);

        DMsNode node;
        std::uint64_t current_addr = !linking_type ? sizeof(header) : header.last_node_addr;

        for (std::uint16_t i = 0; i < header.node_count; ++i) {
            node_addrs.push_back(current_addr);

            file.seekg(current_addr, std::ios::beg);
            file.read(reinterpret_cast<char*>(&node), sizeof(node));

            if (node.next_node_offset == 0) break;
            if (!linking_type)
                current_addr += node.next_node_offset;
            else
                current_addr -= node.next_node_offset;
        }

        // When converting to backward-linked set `last_node_addr` to `node_addrs.back()`
        // When converting to forward-linked keep it as it is (will be set later or is 0
        // for single-node)
        if (target_linking_type && header.node_count > 1)
            header.last_node_addr = node_addrs.back();

        // Reverse `node_addrs` for proper oldest-first ordering
        if (linking_type)
            std::reverse(node_addrs.begin(), node_addrs.end());

        // Update linking type
        header.config |= (target_linking_type ? 1 : 0) << 1;
        // `node` is still alive
        for (std::uint16_t i = 0; i < header.node_count; ++i) {
            file.seekg(node_addrs[i], std::ios::beg);
            file.read(reinterpret_cast<char*>(&node), sizeof(node));

            if (!target_linking_type)
                node.next_node_offset = (i == header.node_count - 1) ?
                    0 : (node_addrs[i + 1] - node_addrs[i]);
            else
                node.next_node_offset = (i == 0) ? 0 : (node_addrs[i] - node_addrs[i - 1]);

            file.seekp(node_addrs[i], std::ios::beg);
            file.write(reinterpret_cast<char*>(&node), sizeof(node));
        }

        file.seekp(0, std::ios::beg);
        // For backward-linked multi-node `last_node_addr` must point to the last node,
        // for single-node it should be 0
        file.write(reinterpret_cast<char*>(&header), sizeof(header));

        return ConvertDMsFileError::OK;

    } catch (const std::ios::failure&) {

        if (file.bad())
            return ConvertDMsFileError::FileFatal;
        else
            return ConvertDMsFileError::FileNonFatal;

    } catch (...) {
        return ConvertDMsFileError::Exception;
    }

}


} // namespace ControlZ

#endif // CONTROLZ_DMS_HPP
