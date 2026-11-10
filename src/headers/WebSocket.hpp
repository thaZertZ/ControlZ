#ifndef CONTROLZ_WEBSOCKET_HPP
#define CONTROLZ_WEBSOCKET_HPP

#include "Common.hpp"
#include "User.hpp"
#include <utility>
#include <cstring>
#include <expected>
#include <optional>
#include <array>
#include <string>
#include <bitset>

namespace ControlZ {

/// @brief Inner namespace for clearer subdivision of structures and
///        implementations. This namespace contains the data structures
///        used by the ControlZ WebSockets subprotocol
namespace ws {


/// @brief A struct holding flags for checking availability of `SequenceID`
///        values for packets
struct SequenceIDTracker {
    /// @brief The flags bitset. For now this is huge, there could be ways of
    ///        making it smaller while keeping the same functionality
    std::bitset<65536> flags;

    /// @brief Reset the state of a particular `SequenceID`
    /// @param i The `SequenceID` to reset the state of
    inline constexpr void reset(SequenceID i) noexcept {
        flags[i] = false;
    }

    /// @brief Mark a particular `SequenceID` as available
    /// @param i The `SequenceID` to mark as available
    inline constexpr void set(SequenceID i) noexcept {
        flags[i] = true;
    }

    /// @brief Retrieve the flag value for a specific `SequenceID`
    /// @param i The `SequenceID` to retrieve the flag value of
    inline constexpr bool at(SequenceID i) noexcept { return flags[i]; }
};


/// @brief A class enum representing the type of a packet
enum class PacketType : std::uint8_t {
    None, // Internal default state, sending this will not do anything
    Auth, // Authenticate a user
    AuthToken, // Authenticate a user with a session token
    Deauth, // Deauthenticate a user
    DM, // Send a private message to a user
    Download, // Download a file from the server
    DownloadChunks, // Download a specific series of chunks from a file
    Explode, // Tell the other end that we ran out of PacketID values
    GetFilename, // Ask the server the name of a file via its AttachmentID
    Heartbeat, // Tell the other end that this end is alive
    InfoChat, // Request information about a public chat
    InfoUser, // Request information about a user
    ManageFriend, // Manage a friend relationship
    Send, // Send a message in a public chat
    Update, // Send new session information
    UpdateChat, // Update a public chat's settings
    UpdateUser, // Update a user's settings
    Upload, // Upload a file to the server
    UploadChunks // Upload a specific series of chunks from a file
};

/// @brief A `PacketType` value carrying the highest numerical value of its backing type
#define CONTROLZ_MAX_PACKET_TYPE_VALUE PacketType::UploadChunks

/// @brief A scoped bitmask enum holding the config flags of a packet
CONTROLZ_MAKE_SCOPED_ENUM (
    PacketFlags, // Type name
    std::uint8_t, // Backing type
    None, // Default value
    None, // Zero value

    // Enum values
    None        = 0,
    RequestNack = 1 << 0,
    RequestAck  = 1 << 1,
    RespondNack = 1 << 2,
    RespondAck  = 1 << 3,
    Fragment    = 1 << 4,
    Response    = 1 << 5,
    Chunk       = 1 << 6
)

/// @brief A struct representing an unpacked `metadata` field of a `PacketHeader` struct
struct PacketHeaderMetadata {
    /// @brief The packet type
    PacketType type = PacketType::None;
    /// @brief The subprotocol version
    std::uint8_t version = 0;

    /// @brief Construct an object from the contents of a `metadata` field
    /// @param meta The `metadata` field
    static inline constexpr PacketHeaderMetadata from_metadata(std::uint8_t meta) noexcept {
        return {
            .type    = static_cast<PacketType>(meta & 0b11111),
            .version =          (std::uint8_t)(meta >> 5)
        };
    }

    /// @brief Turn this object into a valid `metadata` field
    inline constexpr std::uint8_t to_metadata() const noexcept {
        return (std::to_underlying(type) & 0b11111) | (version << 5);
    }

    /// @brief Compare two objects of this type for equality
    /// @param other The other object to compare
    inline constexpr bool operator==(const PacketHeaderMetadata& other) const noexcept {
        return type == other.type && version == other.version;
    }
};

/// @brief A scoped bitmask enum returned when checking for integrity errors in
///        packet encoding and decoding
CONTROLZ_MAKE_SCOPED_ENUM (
    PacketVerifyError, // Type name
    std::uint8_t, // Backing type
    OK, // Default value
    OK, // Zero value

    // Enum values
    OK                  = 0,
    PayloadIsTooShort   = 1 << 0,
    InvalidPacketType   = 1 << 1,
    InvalidVersion      = 1 << 2,
    ZeroUserID          = 1 << 3,
    InvalidAckFlags     = 1 << 4,
    PayloadSizeMismatch = 1 << 5
)

#pragma pack(push, 1)

/// @brief A struct representing the subprotocol header of each packet
struct PacketHeader {
    /// @brief The packet metadata, can be unpacked with a `PacketHeaderMetadata` object
    std::uint8_t metadata = 0;
    /// @brief The packet flags
    PacketFlags flags = PacketFlags::None;
    /// @brief The `SequenceID` associated with this packet
    SequenceID seq_id = 0;
    /// @brief The `SequenceID` associated with the packet being responded to
    SequenceID res_id = 0;
    /// @brief The `UserID` of the user sending this packet
    UserID user_id = 0;
    /// @brief The length of the packet payload in bytes
    std::uint32_t payload_len = 0;

    /// @brief Decode the contents of a payload into a `PacketHeader` object
    /// @param data The payload to decode
    /// @return A correctly-constructed `PacketHeader` object or a scoped bitmask enum
    ///         containing errors if they occurred
    static inline constexpr std::expected<PacketHeader, PacketVerifyError> decode(
            const std::string& data) {
        PacketHeader result;
        PacketVerifyError errors;

        if (data.size() < sizeof(PacketHeader))
            return std::unexpected<PacketVerifyError>(PacketVerifyError::PayloadIsTooShort);

        std::string ordered = data.substr(0, sizeof(result));
        std::memcpy(&result, ordered.data(), sizeof(result));
        // Swap the needed fields and leave the rest untouched
        network_byte_order(result.seq_id);
        network_byte_order(result.res_id);
        network_byte_order(result.payload_len);

        if (data.size() - sizeof(result) != result.payload_len)
            errors |= PacketVerifyError::PayloadSizeMismatch;
        errors |= result.verify(); // Check for other broken invariants

        if (errors) return std::unexpected<PacketVerifyError>(errors);
        return result;
    }

    /// @brief Check for integrity errors inside this `PacketHeader` object
    /// @return A scoped bitmask enum containing errors if there were any
    inline constexpr PacketVerifyError verify() const noexcept {
        PacketVerifyError errors;

        PacketHeaderMetadata meta = PacketHeaderMetadata::from_metadata(metadata);
        if (meta.version != 0) errors |= PacketVerifyError::InvalidVersion;
        if (
            std::to_underlying(meta.type) > std::to_underlying(CONTROLZ_MAX_PACKET_TYPE_VALUE)
            // We don't do this thing below because None can be used as an ACK/NACK-only packet
            //|| meta.type == PacketType::None
        ) errors |= PacketVerifyError::InvalidPacketType;

        if (user_id == 0) errors |= PacketVerifyError::ZeroUserID;

        if (
            ((flags & PacketFlags::RequestAck) && (flags & PacketFlags::RequestNack)) ||
            ((flags & PacketFlags::RespondAck) && (flags & PacketFlags::RespondNack))
        ) errors |= PacketVerifyError::InvalidAckFlags;

        return errors;
    }

    /// @brief Encode the contents of this `PacketHeader` object into a byte payload
    /// @return An encoded payload or a scoped bitmask enum containing errors if there were any
    inline constexpr std::expected<std::string, PacketVerifyError> encode() const {
        if (PacketVerifyError errors = this->verify()) // Check for broken invariants
            return std::unexpected<PacketVerifyError>(errors);

        PacketHeader self = *this; // Copy ourselves so we can byteswap in place
        std::string result(sizeof(self), '\0');

        // The other fields do not need to be swapped
        network_byte_order(self.seq_id); // Individually swap fields
        network_byte_order(self.res_id);
        network_byte_order(self.payload_len);

        std::memcpy(result.data(), &self, sizeof(self));
        return result;
    }

    /// @brief Compare two objects of this type for equality
    /// @param other The other object to compare
    inline constexpr bool operator==(const PacketHeader& other) const noexcept {
        return metadata == other.metadata && flags == other.flags &&
            seq_id == other.seq_id && res_id == other.res_id &&
            payload_len == other.payload_len;
    }
};

/// @brief A pretty useless struct representing a packet of type `None`
struct None {
    static inline constexpr None decode(const std::string&) noexcept {
        return None {};
    }

    // No `verify()`, what did you expect

    inline constexpr std::string encode() const {
        std::string result(0, '\0'); // The second argument is useless but who cares
        return result;
    }
};

/// @brief A struct representing a packet of type `Auth`
struct Auth {
    /// @brief The Argon2id hash of the username
    std::array<char, 32> username_hash = {0};
    /// @brief The Argon2id hash of the password
    std::array<char, 32> pswd_hash = {0};

    /// @brief Decode the contents of a payload into a `Auth` object
    /// @param data The payload to decode
    /// @return A correctly-constructed `Auth` object or a scoped bitmask enum
    ///         containing errors if they occurred
    static inline constexpr std::expected<Auth, PacketVerifyError> decode(const std::string& data) noexcept {
        Auth result;

        std::size_t size = result.username_hash.size() + result.pswd_hash.size();
        if (data.size() != size)
            return std::unexpected<PacketVerifyError>(PacketVerifyError::PayloadSizeMismatch);

        // Copy them separately
        std::memcpy(result.username_hash.data(), data.data(), result.username_hash.size());
        std::memcpy(result.pswd_hash.data(), data.data() + result.username_hash.size(),
            result.username_hash.size());

        // We're not converting endianness since we just have strings
        return result;
    }

    // We don't need a `verify()` method

    /// @brief Encode the contents of this `Auth` object into a byte payload
    /// @return An encoded payload or a scoped bitmask enum containing errors if there were any
    inline constexpr std::string encode() const {
        std::size_t size = username_hash.size() + pswd_hash.size();
        std::string result(size, '\0');

        std::memcpy(result.data(), username_hash.data(), username_hash.size());
        std::memcpy(result.data() + username_hash.size(), pswd_hash.data(), pswd_hash.size());

        return result;
    }

    /// @brief Compare two objects of this type for equality
    /// @param other The other object to compare
    inline constexpr bool operator==(const Auth& other) const noexcept {
        return username_hash == other.username_hash && pswd_hash == other.pswd_hash;
    }
};

/// @brief A struct representing a response packet of type `Auth`
struct AuthResponse {
    /// @brief The `SessionToken` associated with the newly created user session
    SessionToken session_token = {0};

    /// @brief Decode the contents of a payload into a `AuthResponse` object
    /// @param data The payload to decode
    /// @return A correctly-constructed `AuthResponse` object or a scoped bitmask enum
    ///         containing errors if they occurred
    static inline constexpr std::expected<AuthResponse, PacketVerifyError> decode(const std::string& data) noexcept {
        AuthResponse result;

        if (data.size() != result.session_token.size())
            return std::unexpected<PacketVerifyError>(PacketVerifyError::PayloadSizeMismatch);

        std::memcpy(&result, data.data(), result.session_token.size());
        return result;
    }

    // No `verify()` here either

    /// @brief Encode the contents of this `AuthResponse` object into a byte payload
    /// @return An encoded payload or a scoped bitmask enum containing errors if there were any
    inline constexpr std::string encode() const {
        std::string result(session_token.size(), '\0');
        std::memcpy(result.data(), session_token.data(), session_token.size());

        return result;
    }

    /// @brief Compare two objects of this type for equality
    /// @param other The other object to compare
    inline constexpr bool operator==(const AuthResponse& other) const noexcept {
        return session_token == other.session_token;
    }
};

/// @brief A struct representing a packet of type `AuthToken`
struct AuthToken {
    /// @brief The `SessionToken` to use to reauthenticate a session
    SessionToken session_token = {0};

    /// @brief Decode the contents of a payload into a `AuthToken` object
    /// @param data The payload to decode
    /// @return A correctly-constructed `AuthToken` object or a scoped bitmask enum
    ///         containing errors if they occurred
    static inline constexpr std::expected<AuthToken, PacketVerifyError> decode(const std::string& data) noexcept {
        AuthToken result;

        if (data.size() != result.session_token.size())
            return std::unexpected<PacketVerifyError>(PacketVerifyError::PayloadSizeMismatch);

        // Copy members separately
        std::memcpy(result.session_token.data(), data.data(), result.session_token.size());

        return result;
    }

    // No `verify()` needed

    /// @brief Encode the contents of this `AuthToken` object into a byte payload
    /// @return An encoded payload or a scoped bitmask enum containing errors if there were any
    inline constexpr std::string encode() const {
        std::string result(session_token.size(), '\0');

        std::memcpy(result.data(), session_token.data(), session_token.size());

        return result;
    }

    /// @brief Compare two objects of this type for equality
    /// @param other The other object to compare
    inline constexpr bool operator==(const AuthToken& other) const noexcept {
        return session_token == other.session_token;
    }
}; // This is the exact same implementation of `Deauth`

/// @brief A struct representing a packet of type `Deauth`
struct Deauth {
    /// @brief The `SessionToken` that was returned when authenticating this user
    SessionToken session_token = {0};

    /// @brief Decode the contents of a payload into a `Deauth` object
    /// @param data The payload to decode
    /// @return A correctly-constructed `Deauth` object or a scoped bitmask enum
    ///         containing errors if they occurred
    static inline constexpr std::expected<Deauth, PacketVerifyError> decode(const std::string& data) noexcept {
        Deauth result;

        if (data.size() != result.session_token.size())
            return std::unexpected<PacketVerifyError>(PacketVerifyError::PayloadSizeMismatch);

        // Copy members separately
        std::memcpy(result.session_token.data(), data.data(), result.session_token.size());

        return result;
    }

    // No `verify()` needed

    /// @brief Encode the contents of this `Deauth` object into a byte payload
    /// @return An encoded payload or a scoped bitmask enum containing errors if there were any
    inline constexpr std::string encode() const {
        std::string result(session_token.size(), '\0');

        std::memcpy(result.data(), session_token.data(), session_token.size());

        return result;
    }

    /// @brief Compare two objects of this type for equality
    /// @param other The other object to compare
    inline constexpr bool operator==(const Deauth& other) const noexcept {
        return session_token == other.session_token;
    }
};

/// @brief A scoped bitmask enum returned when checking for integrity errors in DM and Send packets
CONTROLZ_MAKE_SCOPED_ENUM (
    MessagePacketVerifyError, // Type name
    std::uint8_t, // Backing type
    OK, // Default value
    OK, // Zero value

    // Enum values
    OK                  = 0,
    PayloadSizeMismatch = 1 << 0,
    ZeroRecipientID     = 1 << 1,
    EmptyMessageData    = 1 << 2,
)

struct DM {
    /// @brief The `UserID` of the recipient
    UserID recipient_id = 0;
    /// @brief Raw `DMsMessage` serialized data. The encoded
    ///        data's contents' endianness is expected to be
    ///        converted to big-endian before encoding
    std::string message;

    static inline constexpr std::expected<DM, MessagePacketVerifyError> decode(const std::string& data) noexcept {
        DM result;

        if (data.size() != sizeof(result))
            return std::unexpected<MessagePacketVerifyError>(MessagePacketVerifyError::PayloadSizeMismatch);

        std::memcpy(&result, data.data(), sizeof(result));
        network_byte_order(result.recipient_id); // Byte order conversion
        return result;
    }

    inline constexpr MessagePacketVerifyError verify() const noexcept {
        MessagePacketVerifyError errors;
        if (recipient_id == 0) errors |= MessagePacketVerifyError::ZeroRecipientID;
        if (message.empty()) errors |= MessagePacketVerifyError::EmptyMessageData;

        return errors;
    }

    inline constexpr std::expected<std::string, MessagePacketVerifyError> encode() const {
        if (MessagePacketVerifyError errors = this->verify())
            return std::unexpected<MessagePacketVerifyError>(errors);

        std::string result(sizeof(recipient_id) + message.size(), '\0');

        UserID swapped = network_byte_order_copy(recipient_id);
        std::memcpy(result.data(), &swapped, sizeof(swapped));
        std::memcpy(result.data() + sizeof(swapped), message.data(), message.size());

        return result;
    }
};

/// @brief A scoped bitmask enum returned when checking for integrity errors in packets
///        related to file handling
CONTROLZ_MAKE_SCOPED_ENUM (
    FilePacketVerifyError, // Type name
    std::uint8_t, // Backing type
    OK, // Default value
    OK, // Zero value

    // Enum values
    OK = 0,
    PayloadSizeMismatch = 1 << 0,
    ZeroAttachmentID    = 1 << 1,
    EmptyFilename       = 1 << 2,
    ZeroChunkSize       = 1 << 3,
    ZeroChunkCount      = 1 << 4,
    EmptyFileData       = 1 << 5,
    NonUniqueIndeces    = 1 << 6,
)

struct Download {
    AttachmentID attachment_id = 0;

    static inline constexpr std::expected<Download, FilePacketVerifyError> decode(const std::string& data) noexcept {
        Download result;
        if (data.size() != sizeof(result.attachment_id))
            return std::unexpected<FilePacketVerifyError>(FilePacketVerifyError::PayloadSizeMismatch);

        std::memcpy(&result.attachment_id, data.data(), sizeof(result.attachment_id));
        network_byte_order(result.attachment_id);
        return result;
    }

    inline constexpr FilePacketVerifyError verify() const noexcept {
        FilePacketVerifyError errors;
        if (attachment_id == 0) errors |= FilePacketVerifyError::ZeroAttachmentID;
        return errors;
    }

    inline constexpr std::expected<std::string, FilePacketVerifyError> encode() const {
        if (FilePacketVerifyError errors = this->verify())
            return std::unexpected<FilePacketVerifyError>(errors);

        AttachmentID swapped = network_byte_order_copy(attachment_id);
        std::string result(sizeof(swapped), '\0');

        std::memcpy(result.data(), &swapped, sizeof(swapped));
        return result;
    }
}; // Same exact implementation as GetFilename

struct DownloadResponse {
    std::uint32_t chunk_size = 0;
    std::uint32_t chunk_count = 0;
    std::string filename;

    static inline constexpr std::expected<DownloadResponse, FilePacketVerifyError> decode(const std::string& data) noexcept {
        DownloadResponse result;
        if (data.size() <= sizeof(result.chunk_size) + sizeof(chunk_count))
            return std::unexpected<FilePacketVerifyError>(FilePacketVerifyError::PayloadSizeMismatch);

        std::memcpy(&result, data.data(), sizeof(result.chunk_size) + sizeof(result.chunk_count));
        network_byte_order(result.chunk_size);
        network_byte_order(result.chunk_count);
        result.filename.resize(data.size() - (sizeof(result.chunk_size) + sizeof(result.chunk_count)), '\0');
        std::memcpy(result.filename.data(), data.data() + sizeof(result.chunk_size) + sizeof(result.chunk_count), result.filename.size());

        return result;
    }

    inline constexpr FilePacketVerifyError verify() const noexcept {
        FilePacketVerifyError errors;
        if (chunk_size == 0) errors |= FilePacketVerifyError::ZeroChunkSize;
        if (chunk_count == 0) errors |= FilePacketVerifyError::ZeroChunkCount;
        if (filename.empty()) errors |= FilePacketVerifyError::EmptyFilename;
        return errors;
    }

    inline constexpr std::expected<std::string, FilePacketVerifyError> encode() const {
        if (FilePacketVerifyError errors = this->verify())
            return std::unexpected<FilePacketVerifyError>(errors);

        std::uint32_t swapped_size = network_byte_order_copy(chunk_size);
        std::uint32_t swapped_count = network_byte_order_copy(chunk_count);
        std::string result(sizeof(swapped_size) + sizeof(swapped_count) + filename.size(), '\0');

        std::memcpy(result.data(), &swapped_size, sizeof(swapped_size));
        std::memcpy(result.data() + sizeof(swapped_size), &swapped_count, sizeof(swapped_count));
        std::memcpy(result.data() + sizeof(swapped_size) + sizeof(swapped_count), filename.data(), filename.size());

        return result;
    }
};

struct DownloadChunk {
    std::uint32_t chunk_index = 0;
    std::string raw_data;

    static inline constexpr std::expected<DownloadChunk, FilePacketVerifyError> decode(const std::string& data) noexcept {
        DownloadChunk result;
        if (data.size() <= sizeof(result.chunk_index))
            return std::unexpected<FilePacketVerifyError>(FilePacketVerifyError::PayloadSizeMismatch);

        std::memcpy(&result.chunk_index, data.data(), sizeof(result.chunk_index));
        network_byte_order(result.chunk_index);
        result.raw_data.resize(data.size() - sizeof(result.chunk_index), '\0');
        std::memcpy(result.raw_data.data(), data.data() + sizeof(result.chunk_index), result.raw_data.size());

        return result;
    }

    inline constexpr FilePacketVerifyError verify() const noexcept {
        FilePacketVerifyError errors;
        if (raw_data.empty()) errors |= FilePacketVerifyError::EmptyFileData;
        return errors;
    }

    inline constexpr std::expected<std::string, FilePacketVerifyError> encode() const {
        if (FilePacketVerifyError errors = this->verify())
            return std::unexpected<FilePacketVerifyError>(errors);

        std::uint32_t swapped = network_byte_order_copy(chunk_index);
        std::string result(sizeof(swapped) + raw_data.size(), '\0');
        std::memcpy(result.data(), &swapped, sizeof(swapped));
        std::memcpy(result.data() + sizeof(swapped), raw_data.data(), raw_data.size());

        return result;
    }
};

struct DownloadChunks {
    std::vector<std::uint32_t> indeces;

    static inline constexpr std::expected<DownloadChunks, FilePacketVerifyError> decode(const std::string& data) noexcept {
        DownloadChunks result;
        // Require that we have a neat size and we have at least one element
        if (data.size() % sizeof(std::uint32_t) != 0 || data.empty())
            return std::unexpected<FilePacketVerifyError>(FilePacketVerifyError::PayloadSizeMismatch);

        result.indeces.resize(data.size() % sizeof(std::uint32_t), 0);
        // Copy elements
        std::memcpy(result.indeces.data(), data.data(), result.indeces.size() * sizeof(std::uint32_t));
        network_byte_order(result.indeces); // First, perform byte swapping to be properly able to sort
        unique_sorted_vec(result.indeces); // Make the indeces sorted and unique
        return result;
    }

    // No `verify()`

    inline constexpr std::string encode() const {
        // We have no other choice but to copy everything
        std::vector<std::uint32_t> swapped = network_byte_order_copy(indeces);
        std::string result(swapped.size() * sizeof(std::uint32_t), '\0');

        // I guess we could get `result`'s constructor to copy stuff for us,
        // but are we allowed to with a type that's not `char`?
        std::memcpy(result.data(), swapped.data(), result.size());
        return result;
    }
};

struct Explode {
    static inline constexpr Explode decode(const std::string&) noexcept {
        return Explode {};
    }

    // No `verify()`

    inline constexpr std::string encode() const {
        std::string result(0, '\0');
        return result;
    }
};

struct GetFilename {
    AttachmentID attachment_id = 0;

    static inline constexpr std::expected<GetFilename, FilePacketVerifyError> decode(const std::string& data) noexcept {
        GetFilename result;

        if (data.size() != sizeof(result.attachment_id))
            return std::unexpected<FilePacketVerifyError>(FilePacketVerifyError::PayloadSizeMismatch);

        std::memcpy(&result.attachment_id, data.data(), sizeof(result.attachment_id));
        network_byte_order(result.attachment_id); // Convert endianness

        return result;
    }

    inline constexpr FilePacketVerifyError verify() const noexcept {
        FilePacketVerifyError errors;
        if (attachment_id == 0) errors |= FilePacketVerifyError::ZeroAttachmentID;
        return errors;
    }

    inline constexpr std::expected<std::string, FilePacketVerifyError> encode() const {
        if (FilePacketVerifyError errors = this->verify())
            return std::unexpected<FilePacketVerifyError>(errors);

        AttachmentID swapped = network_byte_order_copy(attachment_id);
        std::string result(sizeof(swapped), '\0');

        std::memcpy(result.data(), &swapped, sizeof(swapped));
        return result;
    }
};

struct GetFilenameResponse {
    std::string filename;

    static inline constexpr GetFilenameResponse decode(const std::string& data) noexcept {
        return { .filename = data }; // Easiest decode function ever
    }

    // No `verify()`

    inline constexpr std::string encode() const {
        return filename; // Simples encode function ever
    }
};

struct Heartbeat {
    static inline constexpr Heartbeat decode(const std::string&) noexcept {
        return Heartbeat {};
    }

    // No `verify()`

    inline constexpr std::string encode() const {
        std::string result(0, '\0'); // The second argument is useless but who cares
        return result;
    }
};

CONTROLZ_MAKE_SCOPED_ENUM (
    InfoPacketVerifyError, // Type name
    std::uint8_t, // Backing type
    OK, // Default value
    OK, // Zero value

    // Enum values
    OK                  = 0,
    PayloadSizeMismatch = 1 << 0,
    ZeroChatID          = 1 << 1,
    MisalignedList      = 1 << 2,
    ZeroNameLength      = 1 << 3,
    EmptyMembersList    = 1 << 4,
    ZeroUserID          = 1 << 5,
)

struct InfoChat {
    ChatID chat_id = 0;

    static inline constexpr std::expected<InfoChat, InfoPacketVerifyError> decode(const std::string& data) noexcept {
        InfoChat result;
        if (data.size() != sizeof(result.chat_id))
            return std::unexpected<InfoPacketVerifyError>(InfoPacketVerifyError::PayloadSizeMismatch);

        std::memcpy(&result.chat_id, data.data(), sizeof(result.chat_id));
        network_byte_order(result.chat_id);
        return result;
    }

    inline constexpr InfoPacketVerifyError verify() const noexcept {
        InfoPacketVerifyError errors;
        if (chat_id == 0) errors |= InfoPacketVerifyError::ZeroChatID;
        return errors;
    }

    inline constexpr std::expected<std::string, InfoPacketVerifyError> encode() const {
        if (InfoPacketVerifyError errors = this->verify())
            return std::unexpected<InfoPacketVerifyError>(errors);

        ChatID swapped = network_byte_order_copy(chat_id);
        std::string result(sizeof(swapped), '\0');
        std::memcpy(result.data(), &swapped, sizeof(swapped));

        return result;
    }
};

struct InfoChatResponsePolicy {
    bool name = true;
    bool desc = false;
    bool members = false;

    static inline constexpr InfoChatResponsePolicy from_field(std::uint8_t field) noexcept {
        return {
            .name    = (field &  1)       != 0,
            .desc    = (field & (1 << 1)) != 0,
            .members = (field & (1 << 2)) != 0,
        };
    }

    inline constexpr std::uint8_t to_field() const noexcept {
        return (name ? 1 : 0) | (desc ? (1 << 1) : 0) | (members ? (1 << 2) : 0);
    }
};

struct InfoChatResponse {
    std::uint8_t policy_bitmask = InfoChatResponsePolicy().to_field();
    std::uint8_t name_len = 0;
    std::uint16_t desc_len = 0;
    std::string name;
    std::string description;
    std::vector<UserID> members;

    static inline constexpr std::expected<InfoChatResponse, InfoPacketVerifyError> decode(const std::string& data) noexcept {
        InfoChatResponse result;
        if (data.size() <= sizeof(result.policy_bitmask)
            + sizeof(result.name_len)
            + sizeof(result.desc_len)
            + sizeof(UserID) // At least one member
            // We should have name and/or description
        ) return std::unexpected<InfoPacketVerifyError>(InfoPacketVerifyError::PayloadSizeMismatch);

        const char* ptr = data.data();
        std::memcpy(&result, ptr, sizeof(result.policy_bitmask) + sizeof(result.name_len) + sizeof(result.desc_len));
        ptr += sizeof(result.policy_bitmask) + sizeof(result.name_len) + sizeof(result.desc_len);
        network_byte_order(result.desc_len);
        // We don't need to swap anything else
        InfoChatResponsePolicy policy = InfoChatResponsePolicy::from_field(result.policy_bitmask);
        if (policy.name) {
            result.name.resize(result.name_len, '\0');
            std::memcpy(result.name.data(), ptr, result.name_len);
            ptr += result.name_len;
        }
        if (policy.desc) {
            result.description.resize(result.desc_len, '\0');
            std::memcpy(result.description.data(), ptr, result.desc_len);
            ptr += result.desc_len;
        }
        if (policy.members) {
            std::size_t size = data.data() + data.size() - ptr;
            if (size == 0 || size % sizeof(UserID) != 0) return std::unexpected<InfoPacketVerifyError>(InfoPacketVerifyError::MisalignedList);
            result.members.resize(size / sizeof(UserID), 0);
            std::memcpy(result.members.data(), ptr, size);
            network_byte_order(result.members); // Remember to byte swap
        }

        return result;
    }

    inline constexpr InfoPacketVerifyError verify() const noexcept {
        InfoPacketVerifyError errors;
        InfoChatResponsePolicy policy = InfoChatResponsePolicy::from_field(policy_bitmask);
        if (name_len == 0 && policy.name) errors |= InfoPacketVerifyError::ZeroNameLength;
        if (members.empty() && policy.members) errors |= InfoPacketVerifyError::EmptyMembersList;
        return errors;
    }

    inline constexpr std::size_t size() const noexcept {
        InfoChatResponsePolicy policy = InfoChatResponsePolicy::from_field(policy_bitmask);
        return sizeof(policy_bitmask) + sizeof(name_len) + sizeof(desc_len) +
            (policy.name ? name.size() : 0) + (policy.desc ? description.size() : 0) +
            (policy.members ? members.size() * sizeof(UserID) : 0);
    }

    inline constexpr void normalize() noexcept {
        InfoChatResponsePolicy policy = InfoChatResponsePolicy::from_field(policy_bitmask);
        if (name.empty()) {
            policy.name = false;
            name_len = 0;
        } else {
            policy.name = true;
            name_len = name.size(); // I know we could overflow but who cares rn
        }
        if (description.empty()) {
            policy.desc = false;
            desc_len = 0;
        } else {
            policy.desc = true;
            desc_len = description.size(); // Same with this
        }
        if (members.empty()) {
            policy.members = false;
        } else {
            policy.members = true;
        }
        policy_bitmask = policy.to_field();
    }

    inline constexpr std::expected<std::string, InfoPacketVerifyError> encode() {
        this->normalize(); // Let's fix some errors
        if (InfoPacketVerifyError errors = this->verify())
            return std::unexpected<InfoPacketVerifyError>(errors);

        std::string result(this->size(), '\0');
        std::uint16_t swapped_len = network_byte_order_copy(desc_len);
        char* ptr = result.data();

        std::memcpy(ptr, this, sizeof(policy_bitmask) + sizeof(name_len));
        ptr += sizeof(policy_bitmask) + sizeof(name_len);
        std::memcpy(ptr, &swapped_len, sizeof(swapped_len));
        ptr += sizeof(swapped_len);

        InfoChatResponsePolicy policy = InfoChatResponsePolicy::from_field(policy_bitmask);
        if (policy.name) {
            std::memcpy(ptr, name.data(), name_len);
            ptr += name_len; // We can use this because we normalized
        }
        if (policy.desc) {
            std::memcpy(ptr, description.data(), desc_len);
            ptr += desc_len;
        }
        if (policy.members) {
            std::vector<UserID> swapped_members = network_byte_order_copy(members);
            std::memcpy(ptr, swapped_members.data(), swapped_members.size() * sizeof(UserID));
        }

        return result;
    }
};

struct InfoUser {
    UserID user_id = 0;

    static inline constexpr std::expected<InfoUser, InfoPacketVerifyError> decode(const std::string& data) noexcept {
        InfoUser result;
        if (data.size() != sizeof(result.user_id))
            return std::unexpected<InfoPacketVerifyError>(InfoPacketVerifyError::PayloadSizeMismatch);

        std::memcpy(&result.user_id, data.data(), sizeof(result.user_id));
        network_byte_order(result.user_id);
        return result;
    }

    inline constexpr InfoPacketVerifyError verify() const noexcept {
        InfoPacketVerifyError errors;
        if (user_id == 0) errors |= InfoPacketVerifyError::ZeroUserID;
        return errors;
    }

    inline constexpr std::expected<std::string, InfoPacketVerifyError> encode() const {
        if (InfoPacketVerifyError errors = this->verify())
            return std::unexpected<InfoPacketVerifyError>(errors);

        UserID swapped = network_byte_order_copy(user_id);
        std::string result(sizeof(swapped), '\0');
        std::memcpy(result.data(), &swapped, sizeof(swapped));

        return result;
    }
};

struct InfoUserResponsePolicy {
    UserPolicy message_exchange = UserPolicy::Friends;
    UserPolicy friend_reqs = UserPolicy::MutualChatMembers;
    bool username = true;
    bool bio = true;
    bool friends = false;

    static inline constexpr InfoUserResponsePolicy from_field(std::uint8_t field) noexcept {
        return {
            .message_exchange = (field >> 5) & 0b11,
            .friend_reqs      = (field >> 3) & 0b11,
            .username         = (field & (1 << 2)) != 0,
            .bio              = (field & (1 << 1)) != 0,
            .friends          = (field & 1) != 0,
        };
    }

    inline constexpr std::uint8_t to_field() const noexcept {
        return (message_exchange << 5) | (friend_reqs << 3) | (username ? (1 << 2) : 0)
            | (bio ? (1 << 1) : 0) | (friends ? 1 : 0);
    }
};

struct InfoUserResponse {
    Timestamp registration_timestamp = 0;
    std::uint8_t policy_bitmask = InfoUserResponsePolicy().to_field();
    std::uint8_t username_len = 0;
    std::uint16_t bio_len = 0;
    std::string username;
    std::string bio;
    std::vector<UserID> friends;

    static inline constexpr std::expected<InfoUserResponse, InfoPacketVerifyError> decode(const std::string& data) noexcept {
        InfoUserResponse result;
        if (data.size() <= sizeof(result.registration_timestamp)
            + sizeof(result.policy_bitmask)
            + sizeof(username_len)
            + sizeof(result.bio_len)
            // We may have no friends :'(
        ) return std::unexpected<InfoPacketVerifyError>(InfoPacketVerifyError::PayloadSizeMismatch);

        const char* ptr = data.data();
        std::memcpy(&result, ptr, sizeof(result.registration_timestamp) + sizeof(result.policy_bitmask)
            + sizeof(result.username_len) + sizeof(result.bio_len));
        ptr += sizeof(result.registration_timestamp) + sizeof(result.policy_bitmask) + sizeof(result.username_len)
            + sizeof(result.bio_len);

        network_byte_order(result.registration_timestamp);
        network_byte_order(result.bio_len);

        InfoUserResponsePolicy policy = InfoUserResponsePolicy::from_field(result.policy_bitmask);
        if (policy.username) {
            result.username.resize(result.username_len, '\0');
            std::memcpy(result.username.data(), ptr, result.username_len);
            ptr += result.username_len;
        }
        if (policy.bio) {
            result.bio.resize(result.bio_len, '\0');
            std::memcpy(result.bio.data(), ptr, result.bio_len);
            ptr += result.bio_len;
        }
        if (policy.friends) {
            std::size_t size = data.data() + data.size() - ptr;
            if (size % sizeof(UserID) != 0)
                return std::unexpected<InfoPacketVerifyError>(InfoPacketVerifyError::MisalignedList);
            if (size != 0) {
                result.friends.resize(size / sizeof(UserID), 0);
                std::memcpy(result.friends.data(), ptr, size);
                network_byte_order(result.friends); // Remembers to byte swap
            } else; // We have no friends :'(
        }

        return result;
    }

    inline constexpr InfoPacketVerifyError verify() const noexcept {
        InfoPacketVerifyError errors;
        InfoUserResponsePolicy policy = InfoUserResponsePolicy::from_field(policy_bitmask);
        if (username_len == 0 && policy.username) errors |= InfoPacketVerifyError::ZeroNameLength;
        return errors;
    }

    inline constexpr std::size_t size() const noexcept {
        InfoUserResponsePolicy policy = InfoUserResponsePolicy::from_field(policy_bitmask);
        return sizeof(registration_timestamp) + sizeof(policy_bitmask) + sizeof(username_len)
            + sizeof(bio_len) + (policy.username ? username.size() : 0) + (policy.bio ? bio.size() : 0)
            + (policy.friends ? friends.size() * sizeof(UserID) : 0);
    }

    inline constexpr void normalize() noexcept {
        InfoUserResponsePolicy policy = InfoUserResponsePolicy::from_field(policy_bitmask);
        if (username.empty()) {
            policy.username = false;
            username_len = 0;
        } else {
            policy.username = true;
            username_len = username.size();
        }
        if (bio.empty()) {
            policy.bio = false;
            bio_len = 0;
        } else {
            policy.bio = true;
            bio_len = bio.size();
        }
        if (friends.empty()) {
            policy.friends = false;
        } else {
            policy.friends = true;
        }
        policy_bitmask = policy.to_field();
    }

    inline constexpr std::expected<std::string, InfoPacketVerifyError> encode() {
        this->normalize(); // Fix errors
        if (InfoPacketVerifyError errors = this->verify())
            return std::unexpected<InfoPacketVerifyError>(errors);

        std::string result(this->size(), '\0');
        Timestamp swapped_timestamp = network_byte_order_copy(registration_timestamp);
        std::uint16_t swapped_len = network_byte_order_copy(bio_len);
        char* ptr = result.data();

        std::memcpy(ptr, &swapped_timestamp, sizeof(swapped_timestamp));
        ptr += sizeof(swapped_timestamp);
        std::memcpy(ptr, this + sizeof(swapped_timestamp), sizeof(policy_bitmask) + sizeof(username_len));
        ptr += sizeof(policy_bitmask) + sizeof(username_len);
        std::memcpy(ptr, &swapped_len, sizeof(swapped_len));
        ptr += sizeof(swapped_len);

        InfoUserResponsePolicy policy = InfoUserResponsePolicy::from_field(policy_bitmask);
        if (policy.username) {
            std::memcpy(ptr, username.data(), username_len);
            ptr += username_len; // We can use this because we normalized
        }
        if (policy.bio) {
            std::memcpy(ptr, bio.data(), bio_len);
            ptr += bio_len;
        }
        if (policy.friends) {
            std::vector<UserID> swapped_friends = network_byte_order_copy(friends);
            std::memcpy(ptr, swapped_friends.data(), swapped_friends.size() * sizeof(UserID));
        }

        return result;
    }
};

/// @brief A class enum used to represent the action to perform on a friend user
enum class ManageFriendAction : std::uint8_t {
    RequestIfFriend, SendFriendRequest,
    AcceptFriendRequest, RemoveFriend
};

CONTROLZ_MAKE_SCOPED_ENUM (
    ManageFriendVerifyError, // Type name
    std::uint8_t, // Backing type
    OK, // Deafult value
    OK, // Zero value

    // Enum values
    OK                  = 0,
    PayloadSizeMismatch = 1 << 0,
    ZeroTargetUserID    = 1 << 1,
    InvalidAction       = 1 << 2,
    NonZeroPadding      = 1 << 3
)

struct ManageFriend {
    UserID target = 0;
    ManageFriendAction action = ManageFriendAction::RequestIfFriend;
    const std::uint8_t padding = 0;

    static inline constexpr std::expected<ManageFriend, ManageFriendVerifyError>
            decode(const std::string& data) noexcept {

        ManageFriend result;

        if (data.size() != sizeof(result))
            return std::unexpected<ManageFriendVerifyError>(ManageFriendVerifyError::PayloadSizeMismatch);

        // So apparently we have to explicitly say `&result.target` instead of just `&result`
        std::memcpy(&result.target, data.data(), sizeof(result));
        network_byte_order(result.target);
        // Other members are 1 byte in size so we don't swap

        return result;
    }

    inline constexpr ManageFriendVerifyError verify() const noexcept {
        ManageFriendVerifyError errors;

        if (target == 0)
            errors |= ManageFriendVerifyError::ZeroTargetUserID;
        if (std::to_underlying(action) > std::to_underlying(ManageFriendAction::RemoveFriend))
            errors |= ManageFriendVerifyError::InvalidAction;
        if (padding != 0)
            errors |= ManageFriendVerifyError::NonZeroPadding;

        return errors;
    }

    inline constexpr std::expected<std::string, ManageFriendVerifyError> encode() const {
        if (ManageFriendVerifyError errors = this->verify())
            return std::unexpected<ManageFriendVerifyError>(errors);

        ManageFriend self = *this; // Lazy copy?
        network_byte_order(self.target);

        std::string result(sizeof(self), '\0');
        std::memcpy(result.data(), &self, sizeof(self));

        return result;
    }

    inline constexpr bool operator==(const ManageFriend& other) const noexcept {
        return target == other.target && action == other.action && padding == other.padding;
    }
};

struct Send {
    ChatID chat_id = 0;
    std::string message;

    static inline constexpr std::expected<Send, MessagePacketVerifyError> decode(const std::string& data) noexcept {
        Send result;

        if (data.size() <= sizeof(result.chat_id)) // There has to be extra data
            return std::unexpected<MessagePacketVerifyError>(MessagePacketVerifyError::PayloadSizeMismatch);

        std::memcpy(&result.chat_id, data.data(), sizeof(result.chat_id));
        result.message.resize(data.size() - sizeof(result.chat_id));
        std::memcpy(result.message.data(), data.data() + sizeof(result.chat_id), result.message.size());
        network_byte_order(result.chat_id); // Convert endianness

        return result;
    }

    inline constexpr MessagePacketVerifyError verify() const noexcept {
        MessagePacketVerifyError errors;

        if (chat_id == 0) errors |= MessagePacketVerifyError::ZeroRecipientID;
        if (message.empty()) errors |= MessagePacketVerifyError::EmptyMessageData;

        return errors;
    }

    inline constexpr std::expected<std::string, MessagePacketVerifyError> encode() const {
        if (MessagePacketVerifyError errors = this->verify())
            return std::unexpected<MessagePacketVerifyError>(errors);

            ChatID swapped = network_byte_order_copy(chat_id); // Converted endianness
            std::string result(sizeof(swapped) + message.size(), '\0');

        std::memcpy(result.data(), &swapped, sizeof(swapped));
        if (!message.empty()) // Pretty self-explanatory
            std::memcpy(result.data() + sizeof(swapped), message.data(), message.size());

        return result;
    }
};

CONTROLZ_MAKE_SCOPED_ENUM (
    UpdatePacketVerifyError, // Type name
    std::uint16_t, // Backing type
    OK, // Default value
    OK, // Zero value

    // Enum values
    OK                      = 0,
    UpdateChatZeroChatID    = 1 << 0,
    DMsChunkZeroRecipientID = 1 << 1,
    DMsChunkZeroCount       = 1 << 2,
    DMsChunkEmptyData       = 1 << 3,
    FriendReqZeroUserID     = 1 << 4,
    FriendReqInvalidAction  = 1 << 5,
    MessageChunkZeroChatID  = 1 << 6,
    MessageChunkZeroCount   = 1 << 7,
    MessageChunkEmptyData   = 1 << 8,
    PayloadSizeMismatch     = 1 << 9,
)

using EncodedUpdateFriendReq = std::uint32_t;

struct UpdateFriendReq {
    UserID target = 0;
    ManageFriendAction action = ManageFriendAction::SendFriendRequest;
    const std::uint8_t padding = 0;

    inline constexpr UpdateFriendReq& operator=(const UpdateFriendReq& other) noexcept {
        target = other.target;
        action = other.action;
        return *this;
    }

    static inline constexpr UpdateFriendReq from_field(EncodedUpdateFriendReq field) noexcept {
        return {
            .target = network_byte_order_copy(static_cast<UserID>((field & 0xFF00) >> 16)),
            .action = static_cast<ManageFriendAction>((field & 0x00F0) >> 8),
            .padding = 0
        };
    }

    inline constexpr UpdatePacketVerifyError verify() const noexcept {
        UpdatePacketVerifyError errors;
        if (target == 0) errors |= UpdatePacketVerifyError::FriendReqZeroUserID;
        if (std::to_underlying(action) == 0) errors |= UpdatePacketVerifyError::FriendReqInvalidAction;
        return errors;
    }

    inline constexpr EncodedUpdateFriendReq to_field() const noexcept {
        return (network_byte_order_copy(target) << 16) | (std::to_underlying(action) << 8);
    }
};

struct UpdateDMsChunk {
    UserID recipient = 0;
    std::uint16_t message_count = 0;
    std::string raw_data; // Multiple concatenated messages

    static inline constexpr std::expected<UpdateDMsChunk, UpdatePacketVerifyError> decode(const std::string& data) noexcept {
        UpdateDMsChunk result;
        if (data.size() <= sizeof(result.recipient) + sizeof(result.message_count))
            return std::unexpected<UpdatePacketVerifyError>(UpdatePacketVerifyError::PayloadSizeMismatch);

        std::memcpy(&result, data.data(), sizeof(result.recipient) + sizeof(result.message_count));
        network_byte_order(result.recipient);
        network_byte_order(result.message_count);
        result.raw_data.resize(data.size() - sizeof(result.recipient) + sizeof(result.message_count), '\0');
        std::memcpy(result.raw_data.data(), data.data() + sizeof(result.recipient) + sizeof(result.message_count), result.raw_data.size());

        return result;
    }

    inline constexpr UpdatePacketVerifyError verify() const noexcept {
        UpdatePacketVerifyError errors;
        if (recipient == 0) errors |= UpdatePacketVerifyError::DMsChunkZeroRecipientID;
        if (message_count == 0) errors |= UpdatePacketVerifyError::DMsChunkZeroCount;
        if (raw_data.empty()) errors |= UpdatePacketVerifyError::DMsChunkEmptyData;
        return errors;
    }

    inline constexpr std::size_t size() const noexcept {
        return sizeof(recipient) + sizeof(message_count) + raw_data.size();
    }

    inline constexpr std::expected<std::string, UpdatePacketVerifyError> encode() const {
        if (UpdatePacketVerifyError errors = this->verify())
            return std::unexpected<UpdatePacketVerifyError>(errors);

        UserID swapped_id = network_byte_order_copy(recipient);
        std::uint16_t swapped_count = network_byte_order_copy(message_count);
        std::string result(sizeof(swapped_id) + sizeof(swapped_count) + raw_data.size(), '\0');

        std::memcpy(result.data(), &swapped_id, sizeof(swapped_id));
        std::memcpy(result.data() + sizeof(swapped_id), &swapped_count, sizeof(swapped_count));
        std::memcpy(result.data() + sizeof(swapped_id) + sizeof(swapped_count), raw_data.data(), raw_data.size());

        return result;
    }
};

struct UpdateMessageChunk {
    ChatID chat_id = 0;
    std::uint16_t message_count = 0;
    std::string raw_data; // Multiple concatenated messages

    static inline constexpr std::expected<UpdateMessageChunk, UpdatePacketVerifyError> decode(const std::string& data) noexcept {
        UpdateMessageChunk result;
        if (data.size() <= sizeof(result.chat_id) + sizeof(result.message_count))
            return std::unexpected<UpdatePacketVerifyError>(UpdatePacketVerifyError::PayloadSizeMismatch);

        std::memcpy(&result, data.data(), sizeof(result.chat_id) + sizeof(result.message_count));
        network_byte_order(result.chat_id);
        network_byte_order(result.message_count);
        result.raw_data.resize(data.size() - sizeof(result.chat_id) + sizeof(result.message_count), '\0');
        std::memcpy(result.raw_data.data(), data.data() + sizeof(result.chat_id) + sizeof(result.message_count), result.raw_data.size());

        return result;
    }

    inline constexpr UpdatePacketVerifyError verify() const noexcept {
        UpdatePacketVerifyError errors;
        if (chat_id == 0) errors |= UpdatePacketVerifyError::MessageChunkZeroChatID;
        if (message_count == 0) errors |= UpdatePacketVerifyError::MessageChunkZeroCount;
        if (raw_data.empty()) errors |= UpdatePacketVerifyError::MessageChunkEmptyData;
        return errors;
    }

    inline constexpr std::size_t size() const noexcept {
        return sizeof(chat_id) + sizeof(message_count) + raw_data.size();
    }

    inline constexpr std::expected<std::string, UpdatePacketVerifyError> encode() const {
        if (UpdatePacketVerifyError errors = this->verify())
            return std::unexpected<UpdatePacketVerifyError>(errors);

        ChatID swapped_id = network_byte_order_copy(chat_id);
        std::uint16_t swapped_count = network_byte_order_copy(message_count);
        std::string result(sizeof(swapped_id) + sizeof(swapped_count) + raw_data.size(), '\0');

        std::memcpy(result.data(), &swapped_id, sizeof(swapped_id));
        std::memcpy(result.data() + sizeof(swapped_id), &swapped_count, sizeof(swapped_count));
        std::memcpy(result.data() + sizeof(swapped_id) + sizeof(swapped_count), raw_data.data(), raw_data.size());

        return result;
    }
};

struct Update {
    std::uint16_t dms_count = 0;
    std::uint16_t friend_reqs_count = 0;
    std::uint16_t messages_count = 0;
    std::uint32_t dms_size = 0;
    std::vector<std::uint32_t> dms_ptrs;
    std::vector<std::uint32_t> messages_ptrs;
    std::vector<UpdateDMsChunk> dms; // Serialized UpdateDMsChunk objects
    std::vector<UpdateFriendReq> friend_reqs; // These get byte swapped even if the parent structure does it by itself
    std::vector<UpdateMessageChunk> messages; // Serialized UpdateMessageChunk objects

    static inline constexpr std::expected<Update, UpdatePacketVerifyError> decode(const std::string& data) noexcept {
        Update result;
        std::size_t header_size = sizeof(result.dms_count) + sizeof(result.friend_reqs_count) + sizeof(result.messages_count) + sizeof(result.dms_size);
        if (data.size() < header_size)
            return std::unexpected<UpdatePacketVerifyError>(UpdatePacketVerifyError::PayloadSizeMismatch);

        const char* ptr = data.data();
        std::memcpy(&result, ptr, header_size);
        network_byte_order(result.dms_count);
        network_byte_order(result.friend_reqs_count);
        network_byte_order(result.messages_count);
        network_byte_order(result.dms_size);
        ptr += header_size;
        // Get a few sizes ready
        std::size_t dms_ptrs_size = sizeof(std::uint32_t) * result.dms_count;
        std::size_t friend_reqs_size = sizeof(EncodedUpdateFriendReq) * result.friend_reqs_count;
        std::size_t messages_ptrs_size = sizeof(std::uint32_t) * result.messages_count;
        std::size_t messages_size = data.size() - header_size - dms_ptrs_size - messages_ptrs_size - result.dms_size - friend_reqs_size;

        if (result.dms_count != 0) {
            // Fill the addresses vector for DMs
            result.dms_ptrs.resize(result.dms_count, 0);
            result.dms.resize(result.dms_count); // Default construct the objects
            std::vector<std::string> dms_strings(result.dms_count); // Temporary vector
            std::memcpy(result.dms_ptrs.data(), ptr, dms_ptrs_size);
            network_byte_order(result.messages_ptrs);

            const char* base = ptr + dms_ptrs_size + messages_ptrs_size; // Get the base address to our DMs data section
            for (std::size_t i = 0; i < result.dms_count; ++i) {
                std::size_t bytes = 0;

                if (i != result.dms_count - 1) // We're at a normal iteration
                    bytes = result.dms_ptrs[i + 1] - result.dms_ptrs[i]; // Get an offset diff
                else // We're at the last iteration
                    bytes = result.dms_size - result.dms_ptrs[i]; // Same diff

                dms_strings[i].resize(bytes, '\0');
                // Copy from the base plus the current address, up to how many bytes we calculated
                std::memcpy(dms_strings[i].data(), base + result.dms_ptrs[i], bytes);
            }
            // Transition from strings to objects
            for (std::size_t i = 0; i < result.dms_count; ++i) {
                std::expected<UpdateDMsChunk, UpdatePacketVerifyError> err = UpdateDMsChunk::decode(dms_strings[i]);
                if (err)
                    result.dms[i] = *err;
                else
                    return std::unexpected<UpdatePacketVerifyError>(err.error());
            }
            // Don't increment `ptr` because we do it ourselves later, instead we give this to the next stage cleanly
        }
        if (result.friend_reqs_count != 0) {
            ptr += result.dms_size; // Now we have it incremented manually
            result.friend_reqs.resize(result.friend_reqs_count, UpdateFriendReq {}); // Resize and then copy
            for (std::size_t i = 0; i < result.friend_reqs_count; ++i)
                result.friend_reqs[i] = UpdateFriendReq::from_field(ptr[sizeof(EncodedUpdateFriendReq) * i]);
        }
        if (result.messages_count != 0) {
            ptr += friend_reqs_size; // Increment this manually to get to the message data section
            // We use this just to backtrack to the ptrs section
            const char* ptr2 = data.data() + header_size + dms_ptrs_size;
            result.messages_ptrs.resize(result.messages_count, 0);
            result.messages.resize(result.messages_count); // Default construct the objects
            std::vector<std::string> messages_strings(result.messages_count); // Temporary vector
            std::memcpy(result.messages_ptrs.data(), ptr2, messages_ptrs_size);
            network_byte_order(result.messages_ptrs);

            for (std::size_t i = 0; i < result.messages_count; ++i) {
                std::size_t bytes = 0;

                // Calculate the right byte diff
                if (i != result.messages_count - 1)
                    bytes = result.messages_ptrs[i + 1] - result.messages_ptrs[i];
                else
                    bytes = messages_size - result.messages_ptrs[i];

                messages_strings[i].resize(bytes, '\0');
                // Copy the string
                std::memcpy(messages_strings[i].data(), ptr + result.messages_ptrs[i], bytes);
            }
            // Transition from strings to objects
            for (std::size_t i = 0; i < result.messages_count; ++i) {
                std::expected<UpdateMessageChunk, UpdatePacketVerifyError> err = UpdateMessageChunk::decode(messages_strings[i]);
                if (err)
                    result.messages[i] = *err;
                else
                    return std::unexpected<UpdatePacketVerifyError>(err.error());
            }
        }

        return result; // This is by far the most complex decode function
    }

    inline constexpr std::size_t size() const noexcept {
        std::size_t messages_size = 0;
        for (const auto& x : messages)
            messages_size += x.size();
        return sizeof(dms_count) + sizeof(friend_reqs_count)
            + sizeof(messages_count) + sizeof(dms_size)
            + dms_count * sizeof(std::uint32_t) // Pointers
            + messages_count * sizeof(std::uint32_t) // Pointers
            + dms_size
            + friend_reqs_count * sizeof(EncodedUpdateFriendReq)
            + messages_size;
    }

    inline constexpr void normalize() noexcept {
        if (dms.empty()) {
            dms_count = 0;
            dms_size = 0;
            dms_ptrs.clear();
        } else {
            dms_count = dms.size();
            for (const auto& x : dms)
                dms_size += x.size(); // Increment the size gradually
            // We construct the pointers when we encode
        }
        if (friend_reqs.empty()) {
            friend_reqs_count = 0;
        } else {
            friend_reqs_count = friend_reqs.size();
        }
        if (messages.empty()) {
            messages_count = 0;
            messages_ptrs.clear();
        } else {
            messages_count = messages.size();
            // The pointers are the same as before
        }
    }

    inline constexpr std::expected<std::string, UpdatePacketVerifyError> encode() {
        this->normalize(); // Always fix dumb things
        std::string result(this->size(), '\0');
        char* ptr = result.data();

        std::uint16_t swapped_dms_count = network_byte_order_copy(dms_count);
        std::uint16_t swapped_friend_reqs_count = network_byte_order_copy(friend_reqs_count);
        std::uint16_t swapped_messages_count = network_byte_order_copy(messages_count);
        std::uint32_t swapped_dms_size = network_byte_order_copy(dms_size);

        std::memcpy(ptr, &swapped_dms_count, sizeof(swapped_dms_count));
        ptr += swapped_dms_count;
        std::memcpy(ptr, &swapped_friend_reqs_count, sizeof(swapped_friend_reqs_count));
        ptr += swapped_friend_reqs_count;
        std::memcpy(ptr, &swapped_messages_count, sizeof(swapped_messages_count));
        ptr += swapped_messages_count;
        std::memcpy(ptr, &swapped_dms_size, sizeof(swapped_dms_size));
        ptr += swapped_dms_size;

        if (dms_count != 0) {
            std::size_t address = 0;
            dms_ptrs.resize(dms_count, 0);
            for (std::size_t i = 0; i < dms_count; ++i) {
                // Place it already swapped
                dms_ptrs[i] = network_byte_order_copy(address);
                address += dms[i].size(); // Increment gradually
            }

            // They are already swapped so we can write them
            std::memcpy(ptr, dms_ptrs.data(), dms_count * sizeof(std::uint32_t));
            ptr += dms_count * sizeof(std::uint32_t);

            for (const auto& x : dms) {
                std::expected<std::string, UpdatePacketVerifyError> err = x.encode();
                if (!err) return std::unexpected<UpdatePacketVerifyError>(err.error());
                std::string encoded = *err;
                // Write and increment `ptr`
                std::memcpy(ptr, encoded.data(), encoded.size());
                ptr += encoded.size();
            }
        }
        if (friend_reqs_count != 0) {
            std::vector<EncodedUpdateFriendReq> swapped(friend_reqs_count);
            for (std::size_t i = 0; i < friend_reqs_count; ++i)
                swapped[i] = network_byte_order_copy(friend_reqs[i].to_field()); // Convert them in place
            std::memcpy(ptr, swapped.data(), friend_reqs_count * sizeof(EncodedUpdateFriendReq));
            ptr += friend_reqs_count * sizeof(EncodedUpdateFriendReq);
        }
        if (messages_count != 0) {
            std::size_t address = 0;
            messages_ptrs.resize(messages_count, 0);
            for (std::size_t i = 0; i < messages_count; ++i) {
                // Place the addresses already swapped
                messages_ptrs[i] = network_byte_order_copy(address);
                address += messages[i].size();
            }

            // Write directly and increment
            std::memcpy(ptr, messages_ptrs.data(), messages_count * sizeof(std::uint32_t));
            ptr += messages_count * sizeof(std::uint32_t);

            // Final write
            for (const auto& x : messages) {
                // Error checking
                std::expected<std::string, UpdatePacketVerifyError> err = x.encode();
                if (!err) return std::unexpected<UpdatePacketVerifyError>(err.error());
                std::string encoded = *err;
                std::memcpy(ptr, encoded.data(), encoded.size());
                ptr += encoded.size();
            }
        }

        return result;
    }
};

struct UpdateChat {
    ChatID chat_id = 0;
    std::string raw_data;

    static inline constexpr std::expected<UpdateChat, UpdatePacketVerifyError> decode(const std::string& data) noexcept {
        UpdateChat result;
        if (data.size() <= sizeof(result.chat_id)) return std::unexpected<UpdatePacketVerifyError>(UpdatePacketVerifyError::PayloadSizeMismatch);

        std::memcpy(&result.chat_id, data.data(), sizeof(result.chat_id));
        network_byte_order(result.chat_id);
        result.raw_data.resize(data.size() - sizeof(result.chat_id), '\0');
        std::memcpy(result.raw_data.data(), data.data() + sizeof(result.chat_id), result.raw_data.size());

        return result;
    }

    inline constexpr UpdatePacketVerifyError verify() const noexcept {
        UpdatePacketVerifyError errors;
        if (chat_id == 0) errors |= UpdatePacketVerifyError::UpdateChatZeroChatID;
        return errors;
    }

    inline constexpr std::expected<std::string, UpdatePacketVerifyError> encode() const {
        if (UpdatePacketVerifyError errors = this->verify())
            return std::unexpected<UpdatePacketVerifyError>(errors);

        ChatID swapped = network_byte_order_copy(chat_id);
        std::string result(sizeof(swapped) + raw_data.size(), '\0');
        std::memcpy(result.data(), &swapped, sizeof(swapped));
        std::memcpy(result.data() + sizeof(swapped), raw_data.data(), raw_data.size());

        return raw_data; // :)
    }
};

struct UpdateUser {
    std::string raw_data;

    static inline constexpr std::expected<UpdateUser, UpdatePacketVerifyError> decode(const std::string& data) noexcept {
        if (data.empty()) return std::unexpected<UpdatePacketVerifyError>(UpdatePacketVerifyError::PayloadSizeMismatch);
        return UpdateUser { .raw_data = data };
    }

    // No `verify()` even if we need data in the `raw_data` member

    inline constexpr std::string encode() const {
        return raw_data; // :)
    }
}; // Same as UpdateChat

struct Upload {
    std::uint32_t chunk_size = 0;
    std::uint32_t chunk_count = 0;
    std::string filename;

    static inline constexpr std::expected<Upload, FilePacketVerifyError> decode(const std::string& data) noexcept {
        Upload result;
        if (data.size() <= sizeof(result.chunk_size) + sizeof(chunk_count))
            return std::unexpected<FilePacketVerifyError>(FilePacketVerifyError::PayloadSizeMismatch);

        std::memcpy(&result, data.data(), sizeof(result.chunk_size) + sizeof(result.chunk_count));
        network_byte_order(result.chunk_size);
        network_byte_order(result.chunk_count);
        result.filename.resize(data.size() - (sizeof(result.chunk_size) + sizeof(result.chunk_count)), '\0');
        std::memcpy(result.filename.data(), data.data() + sizeof(result.chunk_size) + sizeof(result.chunk_count), result.filename.size());

        return result;
    }

    inline constexpr FilePacketVerifyError verify() const noexcept {
        FilePacketVerifyError errors;
        if (chunk_size == 0) errors |= FilePacketVerifyError::ZeroChunkSize;
        if (chunk_count == 0) errors |= FilePacketVerifyError::ZeroChunkCount;
        if (filename.empty()) errors |= FilePacketVerifyError::EmptyFilename;
        return errors;
    }

    inline constexpr std::expected<std::string, FilePacketVerifyError> encode() const {
        if (FilePacketVerifyError errors = this->verify())
            return std::unexpected<FilePacketVerifyError>(errors);

        std::uint32_t swapped_size = network_byte_order_copy(chunk_size);
        std::uint32_t swapped_count = network_byte_order_copy(chunk_count);
        std::string result(sizeof(swapped_size) + sizeof(swapped_count) + filename.size(), '\0');

        std::memcpy(result.data(), &swapped_size, sizeof(swapped_size));
        std::memcpy(result.data() + sizeof(swapped_size), &swapped_count, sizeof(swapped_count));
        std::memcpy(result.data() + sizeof(swapped_size) + sizeof(swapped_count), filename.data(), filename.size());

        return result;
    }
};

struct UploadResponse {
    AttachmentID attachment_id = 0;

    static inline constexpr std::expected<UploadResponse, FilePacketVerifyError> decode(const std::string& data) noexcept {
        UploadResponse result;
        if (data.size() != sizeof(result.attachment_id))
            return std::unexpected<FilePacketVerifyError>(FilePacketVerifyError::PayloadSizeMismatch);

        std::memcpy(&result.attachment_id, data.data(), sizeof(result.attachment_id));
        network_byte_order(result.attachment_id);
        return result;
    }

    inline constexpr FilePacketVerifyError verify() const noexcept {
        FilePacketVerifyError errors;
        if (attachment_id == 0) errors |= FilePacketVerifyError::ZeroAttachmentID;
        return errors;
    }

    inline constexpr std::expected<std::string, FilePacketVerifyError> encode() const {
        if (FilePacketVerifyError errors = this->verify())
            return std::unexpected<FilePacketVerifyError>(errors);

        AttachmentID swapped = network_byte_order_copy(attachment_id);
        std::string result(sizeof(swapped), '\0');

        std::memcpy(result.data(), &swapped, sizeof(swapped));
        return result;
    }
};

struct UploadChunk {
    std::uint32_t chunk_index = 0;
    std::string raw_data;

    static inline constexpr std::expected<UploadChunk, FilePacketVerifyError> decode(const std::string& data) noexcept {
        UploadChunk result;
        if (data.size() <= sizeof(result.chunk_index))
            return std::unexpected<FilePacketVerifyError>(FilePacketVerifyError::PayloadSizeMismatch);

        std::memcpy(&result.chunk_index, data.data(), sizeof(result.chunk_index));
        network_byte_order(result.chunk_index);
        result.raw_data.resize(data.size() - sizeof(result.chunk_index), '\0');
        std::memcpy(result.raw_data.data(), data.data() + sizeof(result.chunk_index), result.raw_data.size());

        return result;
    }

    inline constexpr FilePacketVerifyError verify() const noexcept {
        FilePacketVerifyError errors;
        if (raw_data.empty()) errors |= FilePacketVerifyError::EmptyFileData;
        return errors;
    }

    inline constexpr std::expected<std::string, FilePacketVerifyError> encode() const {
        if (FilePacketVerifyError errors = this->verify())
            return std::unexpected<FilePacketVerifyError>(errors);

        std::uint32_t swapped = network_byte_order_copy(chunk_index);
        std::string result(sizeof(swapped) + raw_data.size(), '\0');
        std::memcpy(result.data(), &swapped, sizeof(swapped));
        std::memcpy(result.data() + sizeof(swapped), raw_data.data(), raw_data.size());

        return result;
    }
};

struct UploadChunks {
    std::vector<std::uint32_t> indeces;

    static inline constexpr std::expected<UploadChunks, FilePacketVerifyError> decode(const std::string& data) noexcept {
        UploadChunks result;
        // Require that we have a neat size and we have at least one element
        if (data.size() % sizeof(std::uint32_t) != 0 || data.empty())
            return std::unexpected<FilePacketVerifyError>(FilePacketVerifyError::PayloadSizeMismatch);

        result.indeces.resize(data.size() % sizeof(std::uint32_t), 0);
        // Copy elements
        std::memcpy(result.indeces.data(), data.data(), result.indeces.size() * sizeof(std::uint32_t));
        network_byte_order(result.indeces); // First, perform byte swapping to be properly able to sort
        unique_sorted_vec(result.indeces); // Make the indeces sorted and unique
        return result;
    }

    // No `verify()`

    inline constexpr std::string encode() const {
        // We have no other choice but to copy everything
        std::vector<std::uint32_t> swapped = network_byte_order_copy(indeces);
        std::string result(swapped.size() * sizeof(std::uint32_t), '\0');

        // I guess we could get `result`'s constructor to copy stuff for us,
        // but are we allowed to with a type that's not `char`?
        std::memcpy(result.data(), swapped.data(), result.size());
        return result;
    }
};

// Other packet structs go here

#pragma pack(pop)

/// @brief A struct representing a whole packet used by the ControlZ
///        WebSockets subprotocol
struct Packet {
    /// @brief The packet header
    PacketHeader header;
    /// @brief The binary payload
    std::string payload;

    /// @brief Decode the contents of a payload into a `Packet` object
    /// @param data The payload to decode
    /// @return A correctly-constructed `Packet` object or a scoped bitmask enum
    ///         containing errors if they occurred
    static inline constexpr std::expected<Packet, PacketVerifyError> decode(const std::string& data) {
        Packet result;

        if (data.size() < sizeof(PacketHeader)) // We may have no data
            return std::unexpected<PacketVerifyError>(PacketVerifyError::PayloadIsTooShort);

        auto opt = PacketHeader::decode(data);
        if (!opt.has_value())
            return std::unexpected<PacketVerifyError>(opt.error());
        result.header = *opt;

        result.payload = data.substr(sizeof(result.header)); // The rest of the payload
        return result;
    }

    /// @brief Check for integrity errors inside this `Packet` object
    /// @return A scoped bitmask enum containing errors if there were any
    inline constexpr PacketVerifyError verify() const noexcept {
        PacketVerifyError errors;
        errors |= header.verify(); // First part already done
        if (header.payload_len != payload.size())
            errors |= PacketVerifyError::PayloadSizeMismatch;

        return errors;
    }

    /// @brief Encode the contents of this `Packet` object into a byte payload
    /// @return An encoded payload or a scoped bitmask enum containing errors if there were any
    inline constexpr std::expected<std::string, PacketVerifyError> encode() const {
        // Should we error on a `payload.size() == 0`?
        if (PacketVerifyError errors = this->verify())
            return std::unexpected<PacketVerifyError>(errors);

        std::string result(sizeof(header) + payload.size(), '\0');
        // The `encode()` call also calls `verify()` which we proviously checked so it's safe to dereference
        std::string encoded_header = *header.encode();
        std::memcpy(result.data(), encoded_header.data(), encoded_header.size());
        std::memcpy(result.data() + encoded_header.size(), payload.data(), payload.size());

        return result;
    }

    /// @brief Compare two objects of this type for equality
    /// @param other The other object to compare
    inline constexpr bool operator==(const Packet& other) const noexcept {
        return header == other.header && payload == other.payload;
    }
};


} // namespace ws

} // namespace ControlZ

#endif // CONTROLZ_WEBSOCKET_HPP
