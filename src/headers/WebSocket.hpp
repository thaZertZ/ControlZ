#ifndef CONTROLZ_WEBSOCKET_HPP
#define CONTROLZ_WEBSOCKET_HPP

#include "Common.hpp"
#include <utility>
#include <cstring>
#include <expected>
#include <optional>
#include <array>
#include <string>

namespace ControlZ {

/// @brief Inner namespace for clearer subdivision of structures and
///        implementations. This namespace contains the data structures
///        used by the ControlZ WebSockets subprotocol
namespace ws {


/// @brief A class enum representing the type of a packet
enum class PacketType : std::uint8_t {
    None, // Internal default state, sending this will not do anything
    Auth, // Authenticate a user
    AuthToken, // Authenticate a user with a session token
    Deauth, // Deauthenticate a user
    DM, // Send a private message to a user
    Download, // Download a file from the server
    DownloadChunks, // Download a specific series of chunks from a file
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
        PacketVerifyError errors = this->verify(); // Check for broken invariants
        if (errors) return std::unexpected<PacketVerifyError>(errors);

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
    static inline constexpr None decode(const std::string& data) noexcept {
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

struct Heartbeat {
    static inline constexpr Heartbeat decode(const std::string& data) noexcept {
        return Heartbeat {};
    }

    // No `verify()`

    inline constexpr std::string encode() const {
        std::string result(0, '\0'); // The second argument is useless but who cares
        return result;
    }
};

/// @brief A class enum used to represent the action to perform on a friend user
enum class ManageFriendAction : std::uint8_t {
    RequestIfFriend, SendFriendRequest,
    AcceptFriendRequest, RemoveFriend
};

CONTROLZ_MAKE_SCOPED_ENUM (
    ManageFriendVerifyError,
    std::uint8_t,
    OK,
    OK,

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
        ManageFriendVerifyError errors = this->verify();
        if (errors) return std::unexpected<ManageFriendVerifyError>(errors);

        ManageFriend self = *this;
        network_byte_order(self.target);

        std::string result(sizeof(self), '\0');
        std::memcpy(result.data(), &self, sizeof(self));

        return result;
    }

    inline constexpr bool operator==(const ManageFriend& other) const noexcept {
        return target == other.target && action == other.action && padding == other.padding;
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

        auto status = PacketHeader::decode(data);
        if (!status.has_value())
            return std::unexpected<PacketVerifyError>(status.error());
        result.header = *status;

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
        PacketVerifyError errors;
        errors |= this->verify();
        // Should we error on a `payload.size() == 0`?
        if (errors) return std::unexpected<PacketVerifyError>(errors);

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
