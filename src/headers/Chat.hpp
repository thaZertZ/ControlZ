#ifndef CONTROLZ_CHAT_HPP
#define CONTROLZ_CHAT_HPP

#include "Common.hpp"
#include <fstream>
#include <filesystem>
#include <cstring>
#include <optional>
#include <expected>

namespace std {

namespace fs = ::std::filesystem;

} // namespace std

namespace ControlZ {


/// @brief A struct representing the metadata field of a chat file header
struct ChatHeaderMetadata {
    /// @brief The version number
    std::uint8_t version = 0;
    /// @brief Whether the chat requires a password to join
    bool requires_password = false;
    /// @brief Whether non-members can read this chat's name
    bool name_flag = false;
    /// @brief Whether non-members can read this chat's description
    bool desc_flag = false;
    /// @brief Whether non-members can view this chat's members list
    bool members_flag = false;

    /// @brief Pack this struct into a valid metadata field
    std::uint8_t to_metadata() const noexcept {
        std::uint8_t result = 0;

        result |=  version                    << 0x5;
        result |= (requires_password ? 1 : 0) << 0x3;
        result |= (name_flag         ? 1 : 0) << 0x2;
        result |= (desc_flag         ? 1 : 0) << 0x1;
        result |= (members_flag      ? 1 : 0);

        return result;
    }

    /// @brief Unpack a metadata field into an instance of this struct
    /// @param meta The metadata field
    static ChatHeaderMetadata from_metadata(std::uint8_t meta) noexcept {
        return {
            .version           = (std::uint8_t)(meta      >> 0x5),
            .requires_password =               (meta & (1 << 0x3)) != 0,
            .name_flag         =               (meta & (1 << 0x2)) != 0,
            .desc_flag         =               (meta & (1 << 0x1)) != 0,
            .members_flag      =               (meta &  1        ) != 0,
        };
    }

    inline constexpr bool operator==(const ChatHeaderMetadata& other) const noexcept {
        return std::memcmp(this, &other, sizeof(other)) == 0;
    }
};

/// @brief A scoped bitmask enum returned when verifying the
///        state of a `ChatHeader` object
CONTROLZ_MAKE_SCOPED_ENUM (
    ChatHeaderVerifyError,
    std::uint8_t,
    OK,
    OK,

    OK             = 0,
    InvalidMagic   = 1 << 0,
    InvalidVersion = 1 << 1,
    InvalidChatID  = 1 << 2
)

/// @brief A struct representing the header of a chat file
struct ChatHeader {
    /// @brief The 4 magic bytes to identify this format
    const char magic[4] = {'C', 'H', 'A', 'T'};
    /// @brief The `ChatID`. It can't be `0`
    ChatID chat_id = 0;
    /// @brief Metadata about the chat (can be unpacked with `ChatHeaderMetadata`)
    std::uint8_t metadata = 0;
    /// @brief The length in bytes of the chat name
    std::uint8_t name_len = 0;
    /// @brief The length in bytes of the chat description
    std::uint16_t desc_len = 0;
    /// @brief The number of `UserID`s in the members list
    std::uint16_t members_len = 0;

    /// @brief Check for broken invariants inside an instance of this struct
    /// @return A scoped bitmask enum containing errors if there were any
    inline constexpr ChatHeaderVerifyError verify() const noexcept {
        ChatHeaderVerifyError errors;

        if (std::memcmp("CHAT", magic, sizeof(magic)) != 0)
            errors |= ChatHeaderVerifyError::InvalidMagic;
        if (ChatHeaderMetadata::from_metadata(metadata).version != 0)
            errors |= ChatHeaderVerifyError::InvalidVersion;
        if (chat_id == 0)
            errors |= ChatHeaderVerifyError::InvalidChatID;

        return errors;
    }

    inline constexpr bool operator==(const ChatHeader& other) const noexcept {
        return std::memcmp(this, &other, sizeof(other)) == 0;
    }
};

/// @brief A struct representing a chat file
struct ChatFile {
    /// @brief The file header
    ChatHeader header;
    /// @brief The chat name
    std::string name;
    /// @brief The chat description
    std::string description;
    /// @brief The chat's members list
    std::vector<UserID> members;
    /// @brief Optional password data storing 32 bytes of an
    ///        Argon2id salt and 32 bytes of an Argon2id hash,
    ///        only if the header dictated so
    std::optional<std::array<char, 64>> pswd_data;

    /// @brief Return the size in bytes of a hypothetically serialized
    ///        instance of this struct
    inline constexpr std::size_t size() const noexcept {
        std::size_t size = sizeof(header) + name.size() +
            description.size() + members.size() * sizeof(UserID);
        if (pswd_data) size += 64;
        return size;
    }

    /// @brief Serialize an instance of this struct into a string of bytes
    ///        that can be directly written to a file
    inline constexpr std::string serialize() noexcept {
        // Fix invariants (eww)
        std::strncpy(const_cast<char*>(header.magic), "CHAT", sizeof(header.magic));

        // Fix the metadata
        ChatHeaderMetadata meta = ChatHeaderMetadata::from_metadata(header.metadata);
        meta.version = 0;
        meta.requires_password = pswd_data.has_value();
        header.metadata = meta.to_metadata();

        // Resort to a default name
        if (name.empty()) name = "unnamed chat";

        header.name_len = name.size();
        header.desc_len = description.size();
        header.members_len = members.size();

        // Start serializing
        std::string result(this->size(), '\0');
        char* ptr = result.data();

        std::memcpy(ptr, &header, sizeof(header));
        ptr += sizeof(header);

        std::memcpy(ptr, name.data(), header.name_len);
        ptr += header.name_len;
        std::memcpy(ptr, description.data(), header.desc_len);
        ptr += header.desc_len;

        std::memcpy(ptr, members.data(), header.members_len * sizeof(UserID));
        ptr += header.members_len * sizeof(UserID);

        // Only if they both have a value
        if (pswd_data)
            std::memcpy(ptr, pswd_data->data(), 64);

        return result;
    }

    inline constexpr bool operator==(const ChatFile& other) const noexcept {
        return std::memcmp(this, &other, sizeof(header)) == 0 && name == other.name &&
            description == other.description && members == other.members &&
            ((pswd_data.has_value() && other.pswd_data.has_value()) ? *pswd_data == *other.pswd_data : false);
    }
};

/// @brief A scoped bitmask enum returned when creating a chat file
CONTROLZ_MAKE_SCOPED_ENUM (
    CreateChatFileError, // Type name
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

/// @brief Create a chat file
/// @param path The path to the file
/// @param data The file data to write
/// @param force_overwrite If set to `true`, any existing file with the
///                        provided name will be overwritten
/// @return A scoped bitmask enum containing errors if they occurred
[[nodiscard]]
CreateChatFileError create_chat_file(const std::fs::path& path, ChatFile data,
        bool force_overwrite = false) noexcept {

    CreateChatFileError errors;

    if (std::fs::exists(path) && !force_overwrite)
        errors |= CreateChatFileError::FileAlreadyExists;
    if (path.extension() != ".chat")
        errors |= CreateChatFileError::InvalidExtension;

    if (errors) return errors;

    std::ofstream file(path, std::ios::binary);
    if (!file) return CreateChatFileError::FileFatal;
    file.exceptions(std::ios::badbit | std::ios::failbit);

    try {

        file.seekp(0, std::ios::beg);

        std::string bin_data = data.serialize();
        file.write(bin_data.data(), bin_data.size());

    } catch (const std::ios::failure&) {

        if (file.bad())
            return CreateChatFileError::FileFatal;
        else
            return CreateChatFileError::FileNonFatal;

    } catch (...) {
        return CreateChatFileError::Exception;
    }

    return CreateChatFileError::OK;
}

/// @brief Update the contents of a chat file (same as calling `create_chat_file(path, data, true)`)
/// @param path The path to the file
/// @param data The new data to write
/// @return A scoped bitmask enum containing errors if they occurred
[[nodiscard]]
CreateChatFileError update_chat_file(const std::fs::path& path, const ChatFile& data) noexcept {
    return create_chat_file(path, data, true);
}

/// @brief A scoped bitmask enum returned when deserializing a chat file
CONTROLZ_MAKE_SCOPED_ENUM (
    DeserializeChatFileError, // Type name
    std::uint16_t, // Backing type
    OK, // Default value
    OK, // Zero value

    // Enum values
    OK                 = 0,
    FileDoesNotExist   = 1 << 0,
    InvalidExtension   = 1 << 1,
    InvalidMagic       = 1 << 2,
    InvalidVersion     = 1 << 3,
    InvalidChatID      = 1 << 4,
    NoSpaceForPswdData = 1 << 5,
    FileFatal          = 1 << 6,
    FileNonFatal       = 1 << 7,
    Exception          = 1 << 8
)

/// @brief Deserialize the contents of a chat file
/// @param path The path to the file
/// @return A scoped bitmask enum containing errors if they occurred, or a `ChatFile` object containing
///         the deserialized data if no errors occurred
[[nodiscard]]
std::expected<ChatFile, DeserializeChatFileError> deserialize_chat_file(const std::fs::path& path) noexcept {

    DeserializeChatFileError errors;

    if (!std::fs::exists(path)) errors |= DeserializeChatFileError::FileDoesNotExist;
    if (path.extension() != ".chat") errors |= DeserializeChatFileError::InvalidExtension;

    if (errors) return std::unexpected<DeserializeChatFileError>(errors);

    std::size_t filesize = std::fs::file_size(path);

    std::ifstream file(path, std::ios::binary);
    if (!file) return std::unexpected<DeserializeChatFileError>(DeserializeChatFileError::FileFatal);
    file.exceptions(std::ios::badbit | std::ios::failbit);

    try {

        file.seekg(0, std::ios::beg);

        ChatFile result;
        file.read(reinterpret_cast<char*>(&result.header), sizeof(result.header));

        ChatHeaderVerifyError status = result.header.verify();
        if (status & ChatHeaderVerifyError::InvalidMagic) errors |= DeserializeChatFileError::InvalidMagic;
        if (status & ChatHeaderVerifyError::InvalidVersion) errors |= DeserializeChatFileError::InvalidVersion;
        if (status & ChatHeaderVerifyError::InvalidChatID) errors |= DeserializeChatFileError::InvalidChatID;

        if (errors) return std::unexpected<DeserializeChatFileError>(errors);

        result.name.resize(result.header.name_len, '\0');
        result.description.resize(result.header.desc_len, '\0');
        result.members.resize(result.header.members_len, 0);

        file.read(result.name.data(), result.header.name_len);
        file.read(result.description.data(), result.header.desc_len);
        // Don't forget the size
        file.read(reinterpret_cast<char*>(result.members.data()), result.header.members_len * sizeof(UserID));

        // We have the optional data if we say it in the header
        if (ChatHeaderMetadata::from_metadata(result.header.metadata).requires_password) {
            //auto a = file.tellg();
            if (filesize - file.tellg() != 64) // But check first for enough space
                return std::unexpected<DeserializeChatFileError>(DeserializeChatFileError::NoSpaceForPswdData);
            result.pswd_data = std::make_optional<std::array<char, 64>>({0}); // Fill with zeroes
            file.read(result.pswd_data->data(), 64);
        }

        return result;

    } catch (const std::ios::failure&) {

        if (file.bad())
            return std::unexpected<DeserializeChatFileError>(DeserializeChatFileError::FileFatal);
        else
            return std::unexpected<DeserializeChatFileError>(DeserializeChatFileError::FileNonFatal);

    } catch (...) {
        return std::unexpected<DeserializeChatFileError>(DeserializeChatFileError::Exception);
    }
}


} // namespace ControlZ

#endif // CONTROLZ_CHAT_HPP
