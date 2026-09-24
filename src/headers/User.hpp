#ifndef CONTROLZ_USER_HPP
#define CONTROLZ_USER_HPP

#include "Common.hpp"
#include <cstring>
#include <filesystem>
#include <fstream>
#include <expected>

namespace std {

namespace fs = ::std::filesystem;

} // namespace std

namespace ControlZ {


/// @brief A scoped enum representing a policy used to determine if
///        a user has access to information about another user
CONTROLZ_MAKE_SCOPED_ENUM (
    UserPolicy, // Type name
    std::uint8_t, // Backing type
    Friends, // Default value
    NoOne, // Zero value

    // Enum values
    NoOne             = 0,
    Friends           = 1,
    MutualChatMembers = 2,
    Everyone          = 3
)

/// @brief A scoped enum representing the level of privilege a user has
CONTROLZ_MAKE_SCOPED_ENUM (
    ModLevel, // Type name
    std::uint8_t, // Backing type
    Normal, // Default value
    Normal, // Zero value

    // Enum values
    Normal          = 0,
    LocalModerator  = 1,
    GlobalModerator = 2,
    Administrator   = 3
)

/// @brief A scoped enum representing the status of a user's account
CONTROLZ_MAKE_SCOPED_ENUM (
    UserStatus, // Type name
    std::uint8_t, // Backing type
    Active, // Default value
    Active, // Zero value

    // Enum values
    Active    = 0,
    Warned    = 1,
    Suspended = 2,
    Banned    = 3
)

/// @brief A struct representing the metadata field of a `UserHeader`
struct UserHeaderMetadata {
    /// @brief The format version
    std::uint8_t version = 0;
    /// @brief The moderation level
    std::uint8_t mod_level = 0;
    /// @brief The policy that users should follow to read this user's bio
    std::uint8_t bio_policy = 0;
    /// @brief The policy that users should follow to read this user's friend list
    std::uint8_t friends_policy = 0;
    /// @brief The status of this user's account
    std::uint8_t user_status = 0;

    /// @brief Pack this struct into a valid metadata field for a `UserHeader`
    inline constexpr std::uint16_t to_metadata() const noexcept {
        /*
        F E D C B A 9 8 7 6 5 4 3 2 1 0
        -------------------------------
        v v v m m i i r r s s x x x x x

        v  :  Version number
        m  :  Moderation level
        i  :  Bio policy
        r  :  Friends list policy
        s  :  User status
        x  :  Reserved for future use
        */

        std::uint16_t result = 0;
        result |= user_status << 0x5;
        result |= friends_policy << 0x7;
        result |= bio_policy << 0x9;
        result |= mod_level << 0xB;
        result |= version << 0xD;
        return result;
    }

    /// @brief Unpack a metadata field from a `UserHeader`
    /// @param meta The metadata field
    static inline constexpr UserHeaderMetadata from_metadata(std::uint16_t meta) noexcept {
        return {
            .version        = (std::uint8_t) (meta >> 0xD),
            .mod_level      = (std::uint8_t)((meta >> 0xB) & 0b11),
            .bio_policy     = (std::uint8_t)((meta >> 0x9) & 0b11),
            .friends_policy = (std::uint8_t)((meta >> 0x7) & 0b11),
            .user_status    = (std::uint8_t)((meta >> 0x5) & 0b11)
        };
    }
};

/// @brief A scoped bitmask enum returned when verifying the integrity of a `UserHeader`
CONTROLZ_MAKE_SCOPED_ENUM (
    VerifyUserHeaderError, // Type name
    std::uint8_t, // Backing type
    OK, // Default value
    OK, // Zero value

    // Enum values
    OK              = 0,
    InvalidMagic    = 1 << 0,
    InvalidVersion  = 1 << 1,
    NonZeroPaddings = 1 << 2
)

#pragma pack(push, 1)

/// @brief A struct representing the header of a user file
struct UserHeader {
    /// @brief The three magic bytes `USR`
    const char magic[3] = {'U', 'S', 'R'};
    /// @brief A padding null byte
    const std::uint8_t padding1 = 0;
    /// @brief The `UserID` of this user
    UserID user_id = 0;
    /// @brief Contains metadata about the user and their data policies
    std::uint16_t metadata = 0;
    /// @brief The timestamp in seconds, from 1/1/2027 at 00:00 UTC, on which the user
    ///        was registered
    Timestamp registration_timestamp = 0;
    /// @brief The salt value used when hashing the user's password
    char argon2id_salt[32] = {0};
    /// @brief The hashed user password
    char argon2id_hash[32] = {0};
    /// @brief The length of the username bytes after this header
    std::uint8_t username_len = 0;
    /// @brief The length of the bio bytes after the username ones
    std::uint16_t bio_len = 0;
    /// @brief Another padding null byte
    const std::uint8_t padding2 = 0;

    /// @brief Check for broken invariants in a `UserHeader` instance
    /// @return A scoped bitmask enum with the status of the check
    inline constexpr VerifyUserHeaderError verify() const noexcept {
        VerifyUserHeaderError errors;

        if (magic[0] != 'U' || magic[1] != 'S' || magic[2] != 'R')
            errors |= VerifyUserHeaderError::InvalidMagic;
        if (UserHeaderMetadata::from_metadata(metadata).version != 0)
            errors |= VerifyUserHeaderError::InvalidVersion;
        if (padding1 != 0 || padding2 != 0)
            errors |= VerifyUserHeaderError::NonZeroPaddings;

        return errors;
    }
};

#pragma pack(pop)

struct UserFile {
    /// @brief The file header
    UserHeader header;
    /// @brief The username string
    std::string username;
    /// @brief The biography string
    std::string bio;
    /// @brief A list of `UserID`s of this user's friends
    std::vector<UserID> friends;

    /// @brief Return the size of a hypothetically serialized `UserFile` object
    inline constexpr std::size_t size() const noexcept {
        std::size_t result = sizeof(header) + username.size() + bio.size() + friends.size() * sizeof(UserID);
        return result % 2 == 0 ? result : result + 1; // Account for padding
    }

    /// @brief Serialize this object into a string of bytes, adding an
    ///        extra null byte to align to two bytes if needed
    inline constexpr std::string serialize() const noexcept {
        std::string result(this->size(), '\0');
        char* ptr = result.data();

        std::memcpy(ptr, &header, sizeof(header));
        ptr += sizeof(header);

        if (username.size() != 0)
            std::memcpy(ptr, username.data(), username.size());
        ptr += username.size();

        if (bio.size() != 0)
            std::memcpy(ptr, bio.data(), bio.size());
        ptr += bio.size();

        if (friends.size() != 0)
            std::memcpy(ptr, friends.data(), friends.size() * sizeof(UserID));

        return result;
    }
};

/// @brief A scoped bitmask enum returned when creating a user file
CONTROLZ_MAKE_SCOPED_ENUM (
    CreateUserFileError, // Type name
    std::uint8_t, // Backing type
    OK, // Default value
    OK, // Zero value

    // Enum values
    OK                = 0,
    InvalidExtension  = 1 << 0,
    InvalidMagic      = 1 << 1,
    InvalidVersion    = 1 << 2,
    NonZeroPaddings   = 1 << 3,
    FileAlreadyExists = 1 << 4,
    FileFatal         = 1 << 5,
    FileNonFatal      = 1 << 6,
    Exception         = 1 << 7
)

/// @brief Create a user file
/// @param path The path to the new file
/// @param data The user data to write
/// @param force_overwrite If set to `true` an existing file with the
///                        provided name will be overwritten, else this
///                        function will return early
/// @return A scoped bitmask enum containing errors if they occurred
CreateUserFileError create_user_file(const std::fs::path& path, const UserFile& data,
        bool force_overwrite = false) noexcept {

    CreateUserFileError errors;

    // Basic checks
    if (path.extension() != ".usr")
        errors |= CreateUserFileError::InvalidExtension;

    if (std::fs::exists(path) && !force_overwrite)
        errors |= CreateUserFileError::FileAlreadyExists;

    // Check if we broke invariants
    VerifyUserHeaderError status = data.header.verify();
    if (status & VerifyUserHeaderError::InvalidMagic)
        errors |= CreateUserFileError::InvalidMagic;
    if (status & VerifyUserHeaderError::InvalidVersion)
        errors |= CreateUserFileError::InvalidVersion;
    if (status & VerifyUserHeaderError::NonZeroPaddings)
        errors |= CreateUserFileError::NonZeroPaddings;

    if (errors) return errors; // From now on we have no caller-dependent errors

    std::ofstream file(path, std::ios::binary);
    if (!file) return CreateUserFileError::FileFatal;
    file.exceptions(std::ios::badbit | std::ios::failbit);

    try {

        file.seekp(0, std::ios::beg);

        std::string bin_data = data.serialize();
        file.write(bin_data.data(), bin_data.size()); // Very simple

    } catch (const std::ios::failure&) {

        if (file.bad())
            return CreateUserFileError::FileFatal;
        else
            return CreateUserFileError::FileNonFatal;

    } catch (...) {
        return CreateUserFileError::Exception;
    }

    return CreateUserFileError::OK;

}

/// @brief Just an alias for calling `create_user_file` with `force_overwrite` set to `true`.
///        This is here just to make code more readable and easier to reason about
/// @param path The path to the file to update
/// @param data The new user data to write
/// @return A scoped bitmask enum containing errors if they occurred
inline CreateUserFileError update_user_file(const std::fs::path& path, const UserFile& data)
        noexcept {
    return create_user_file(path, data, true);
}

/// @brief A scoped bitmask enum returned when deserializing a user file
CONTROLZ_MAKE_SCOPED_ENUM (
    DeserializeUserFileError, // Type name
    std::uint16_t, // Backing
    OK, // Default value
    OK, // Zero value

    // Enum values
    OK               = 0,
    FileDoesNotExist = 1 << 0,
    MisalignedData   = 1 << 1,
    InvalidExtension = 1 << 2,
    InvalidMagic     = 1 << 3,
    InvalidVersion   = 1 << 4,
    NonZeroPaddings  = 1 << 5,
    FileFatal        = 1 << 6,
    FileNonFatal     = 1 << 7,
    Exception        = 1 << 8
)

/// @brief Deserialize the contents of a user file into a `UserFile` object
/// @param path THe path to the file to deserialize
/// @return The deserialized object if errors did not occur, otherwise, a scoped
///         bitmask enum containing errors will be returned
inline std::expected<UserFile, DeserializeUserFileError> deserialize_user_file(
        const std::fs::path& path) noexcept {

    DeserializeUserFileError errors;

    if (!std::fs::exists(path)) errors |= DeserializeUserFileError::FileDoesNotExist;
    if (path.extension() != ".usr") errors |= DeserializeUserFileError::InvalidExtension;
    if (std::fs::file_size(path) % 2 != 0) errors |= DeserializeUserFileError::MisalignedData;

    std::ifstream file(path, std::ios::binary);
    if (!file) return std::unexpected<DeserializeUserFileError>(DeserializeUserFileError::FileFatal);
    file.exceptions(std::ios::badbit | std::ios::failbit);

    try {

        file.seekg(0, std::ios::beg);

        UserFile data;
        // Only read the first bit
        file.read(reinterpret_cast<char*>(&data), sizeof(UserHeader));

        // Again check for broken invariants
        VerifyUserHeaderError status = data.header.verify();
        if (status & VerifyUserHeaderError::InvalidMagic)
            errors |= DeserializeUserFileError::InvalidMagic;
        if (status & VerifyUserHeaderError::InvalidVersion)
            errors |= DeserializeUserFileError::InvalidVersion;
        if (status & VerifyUserHeaderError::NonZeroPaddings)
            errors |= DeserializeUserFileError::NonZeroPaddings;

        if (errors) return std::unexpected<DeserializeUserFileError>(errors);

        // Copy the username only if there is one (lol this check shouldn't even exist)
        if (data.header.username_len != 0) {
            data.username.resize(data.header.username_len, '\0');
            file.read(data.username.data(), data.header.username_len);
        }

        // Copy the bio if it exists
        if (data.header.bio_len != 0) {
            data.bio.resize(data.header.bio_len, '\0');
            file.read(data.bio.data(), data.header.bio_len);
        }

        std::size_t current_size = sizeof(UserHeader) +
            data.header.username_len + data.header.bio_len;
        // This means we have a friends list
        if (data.size() != current_size) {
            data.friends.resize(data.size() - current_size, 0);

            file.read(reinterpret_cast<char*>(data.friends.data()),
                data.friends.size() * sizeof(UserID));
        }

        return data; // The expected result

    } catch (const std::ios::failure&) {

        if (file.bad())
            return std::unexpected<DeserializeUserFileError>(DeserializeUserFileError::FileFatal);
        else
            return std::unexpected<DeserializeUserFileError>(DeserializeUserFileError::FileNonFatal);

    } catch (...) {
        return std::unexpected<DeserializeUserFileError>(DeserializeUserFileError::Exception);
    }
}


} // namespace ControlZ

#endif // CONTROLZ_USER_HPP
