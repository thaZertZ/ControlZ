#ifndef CONTROLZ_ENVELOPE_HPP
#define CONTROLZ_ENVELOPE_HPP

#include "Common.hpp"
#include <expected>
#include <optional>
#include <vector>
#include <string>
#include <cstring>

namespace ControlZ {


/// @brief A struct representing an unpacked version of the metadata
///        field in a DMsMessage struct
struct DMsMessageMetadata {
    /// @brief If set to `true` the message is an edit
    bool edit = false;
    /// @brief The format version
    std::uint8_t version = 0;
    /// @brief The number of attachments (max 7)
    std::uint8_t attachment_count = 0;
    /// @brief If set to true the message is a reply
    bool reply = false;
    /// @brief The encoding scheme used to encode the length of the message payload
    bool length_encoding = false;
    /// @brief The decoded length of the payload. NOT encoded
    std::uint16_t length = 0;

    /// @brief Serialize a struct of this type to a valid metadata field
    ///        (the length is encoded here, it does not need to be encoded manually)
    inline constexpr std::uint16_t to_metadata() const noexcept {

        /*
        F E D C B A 9 8 7 6 5 4 3 2 1 0
        -------------------------------
        t v v k k k r x z z z z z z z z

        t  :  edit
        v  :  version
        k  :  attachment_count
        r  :  reply
        x  :  length_encoding
        z  :  length
        */

        std::uint16_t result = 0;
        if (length_encoding) // Long encoding
            result |= (length + 7) / 8; // No shifts, all 8 bits
        else // Short encoding
            result |= (length + 3) / 4; // Same

        result |= (length_encoding ? 1 : 0) << 0x8; // 1 bit
        result |= (reply           ? 1 : 0) << 0x9; // 1 bit
        result |=  attachment_count         << 0xA; // 3 bits
        result |=  version                  << 0xD; // 2 bits
        result |= (edit            ? 1 : 0) << 0xF; // 1 bit

        return result;
    }

    /// @brief Create a metadata struct out of a metadata field taken from a DMsMessageHeader
    /// @param meta The field to unpack
    static inline constexpr DMsMessageMetadata from_metadata(std::uint16_t meta) noexcept {
        return {
            .edit             =                (meta                   >> 0xF)    != 0,
            .version          = (std::uint8_t)((meta & (0b11  << 0xD)) >> 0xD),
            .attachment_count = (std::uint8_t)((meta & (0b111 << 0xA)) >> 0xA),
            .reply            =               ((meta & (0b1 << 0x9))   >> 9)      != 0,
            .length_encoding  =                (meta & (0b1 << 0x8))              != 0,
            .length           =                (meta & (0b1 << 0x8)) ? // Which length encoding?
                               (std::uint16_t)((meta & 0xFF) * 8)    : // Long encoding
                               (std::uint16_t)((meta & 0xFF) * 4)      // Short encoding
        };
    }
};

#pragma pack(push, 1)

/// @brief A struct representing the format header of a DMsMessage
struct DMsMessageHeader {
    /// @brief The timestamp of the message (if the message is an edit,
    ///        this must be the timestamp of the original message being edited)
    Timestamp timestamp = 0;
    /// @brief The UserID of the sender
    UserID user_id = 0;
    /// @brief The metadata associated with this message (easier to edit with DMsMessageMetadata)
    /// ```txt
    /// F E D C B A 9 8 7 6 5 4 3 2 1 0
    /// -------------------------------
    /// t v v k k k r x z z z z z z z z
    /// 
    /// t  :  edit flag
    /// v  :  version
    /// k  :  attachment_count
    /// r  :  reply flag
    /// x  :  length_encoding
    /// z  :  length
    /// ```
    std::uint16_t metadata = 0;

    /// @brief Return the length encoding held by the `metadata` field
    inline constexpr bool length_encoding() const noexcept {
        return DMsMessageMetadata::from_metadata(metadata).length_encoding; // Shorter way
    }
};

/// @brief A struct representing the dynamic reply payload in a DMsMessage
struct DMsMessageReplies {
    /// @brief The number of replies biased by `-1` (eg. `0` encodes `1`)
    std::uint8_t reply_count = 0;
    /// @brief The name says it all :)
    std::uint8_t padding_null_byte = 0;
    /// @brief A pointer to a heap-allocated array of `Timestamp`s.
    ///        Do NOT serialize this struct by `memcpy`ing, instead
    ///        call `serialize()` to include the actual pointed-to data
    std::vector<Timestamp> timestamps;

    /// @brief Construct an object specifying the number of timestamps to allocate
    /// @param count The number of timestamps
    DMsMessageReplies(std::uint8_t count) {
        reply_count = count == 0 ? 0 : count - 1;
        padding_null_byte = 0;
        timestamps.resize(reply_count, 0);
    }

    /// @brief Return `true` if there are allocated timestamps. Note that the
    ///        `reply_count` member will always encode a non-zero value, but the
    ///        actual `timestamps` pointer could be `nullptr`
    inline constexpr operator bool() const noexcept {
        return timestamps.data() != nullptr;
    }

    /// @brief Return the size that a serialized payload would have
    inline constexpr std::size_t size() const noexcept {
        return 2 + (reply_count + 1 /* biased encoding */) * sizeof(Timestamp);
    }

    /// @brief Return a string of bytes containing a serialized version of this struct
    inline constexpr std::string serialize() const noexcept {
        std::string result(this->size(), '\0'); // Resize and zero out

        std::memcpy(result.data(), this, 2); // Copy the first two bytes
        
        if (!timestamps.data()) return result; // The timestamps are all zero since we haven't allocated anything
        std::memcpy(result.data() + 2, timestamps.data(), timestamps.size() * sizeof(Timestamp));

        return result;
    }
};

/// @brief A struct representing a file attachment in a DMsMessage
struct DMsMessageAttachment {
    /// @brief The `AttachmentID` associated with the attachment
    AttachmentID id = 0;
    /// @brief The file extension of the attachment. All major file extensions
    ///        are supported, but if a not supported one is encountered, this
    ///        value is set to `ControlZ::FileExtension::Other`, indicating that
    ///        a client must ask the server for the original extension of the file,
    ///        contained in a mapping file
    FileExtension extension = FileExtension::None;

    /// @brief Construct an object with an `AttachmentID`
    /// @param aid The `AttachmentID`
    DMsMessageAttachment(AttachmentID aid) noexcept
        : id(aid) {}
};

/// @brief A struct representing the message payload of a DMsMessage
struct DMsMessagePayload {
    /// @brief The raw text data
    std::string data;

    /// @brief Add a maximum of 3 padding null bytes to the message
    ///        buffer to allow encoding with the short length encoding
    inline constexpr void short_encode() {
        std::uint8_t exceeding = data.size() % 4;
        if (exceeding == 0) return;
        data.append(4 - exceeding, '\0');
    }

    /// @brief Add a maximum of 7 padding null bytes to the message
    ///        buffer to allow encoding with the long length encoding
    inline constexpr void long_encode() {
        std::uint8_t exceeding = data.size() % 8;
        if (exceeding == 0) return;
        data.append(8 - exceeding, '\0');
    }
};

#pragma pack(pop)

/// @brief A struct representing a full DMsMessage
struct DMsMessage {
    /// @brief The message header
    DMsMessageHeader header;
    /// @brief An optional reply payload
    std::optional<DMsMessageReplies> replies;
    /// @brief An optional attachment payload
    std::optional<std::vector<DMsMessageAttachment>> attachments;
    /// @brief An optional message payload
    std::optional<DMsMessagePayload> payload;

    /// @brief Create a string of bytes representing a supposedly valid
    ///        message from this instance
    inline constexpr std::string serialize() {
        std::string output;

        std::size_t final_size = sizeof(header);
        if (replies) final_size += replies.value().size();
        if (attachments)
            final_size += attachments.value().size() * sizeof(DMsMessageAttachment);
        if (payload) final_size += payload.value().data.size();

        output.resize(final_size, 0);
        char* ptr = output.data();

        std::memcpy(ptr, &header, sizeof(header));
        ptr += sizeof(header);

        if (replies) {
            std::string data = replies.value().serialize();
            std::memcpy(ptr, data.data(), data.size());
            ptr += data.size();
        }

        if (attachments) {
            std::size_t bytes = attachments.value().size() * sizeof(attachments.value().front());
            std::memcpy(ptr, attachments.value().data(), bytes);
            ptr += bytes;
        }

        if (payload) {
            if (header.length_encoding())
                payload.value().long_encode();
            else
                payload.value().short_encode();

            std::memcpy(ptr, payload.value().data.data(), payload.value().data.size());
        }

        return output;
    }
};


} // namespace ControlZ

#endif // CONTROLZ_ENVELOPE_HPP
