#ifndef CONTROLZ_ENVELOPE_HPP
#define CONTROLZ_ENVELOPE_HPP

#include "Common.hpp"
#include <expected>
#include <optional>
#include <vector>
#include <string>
#include <cstring>
#include <utility> // std::to_underlying()

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

    /// @brief Return a length value encoded with a provided length encoding
    /// @param length The value to encode
    /// @param length_encoding The specified encoding
    static inline constexpr std::uint16_t encoded_length(std::uint16_t length, bool length_encoding) noexcept {
        return length * (
            length_encoding ? 8 : 4
        );
    }

    /// @brief Return a length value decoded with a provided length encoding
    /// @param length The value to decode
    /// @param length_encoding The specified encoding
    static inline constexpr std::uint16_t decoded_length(std::uint16_t length, bool length_encoding) noexcept {
        return (
            length_encoding        ? // Which length encoding?
            ((length + 7) / 8) * 8 : // Long encoding
            ((length + 3) / 4) * 4   // Short encoding
        );
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

/// @brief Convert in-place the endianness of a `DMsMessageHeader` if the host is little endian
/// @param header The header to convert
template <>
inline constexpr void network_byte_order(DMsMessageHeader& header) noexcept {
    if constexpr (std::endian::native != std::endian::little) return;
    header.timestamp = std::byteswap(header.timestamp);
    header.user_id = std::byteswap(header.user_id);
    header.metadata = std::byteswap(header.metadata);
}

/// @brief Return a value with the converted endianness of another value
///        of type `DMsMessageHeader` if the host is little endian
/// @param header The header to convert
template <>
inline constexpr DMsMessageHeader network_byte_order_copy(const DMsMessageHeader& header) noexcept {
    if constexpr (std::endian::native != std::endian::little) return header;
    return {
        .timestamp = std::byteswap(header.timestamp),
        .user_id = std::byteswap(header.user_id),
        .metadata = std::byteswap(header.metadata)
    };
}

#pragma pack(pop) // We don't need it for DMsMessageReplies since it has a vector

/// @brief A struct representing the dynamic reply payload in a DMsMessage
struct DMsMessageReplies {
    /// @brief The number of replies biased by `-1` (eg. `0` encodes `1`)
    std::uint8_t reply_count = 0;
    /// @brief The name says it all :)
    const std::uint8_t padding_null_byte = 0;
    /// @brief A pointer to a heap-allocated array of `Timestamp`s.
    ///        Do NOT serialize this struct by `memcpy`ing, instead
    ///        call `serialize()` to include the actual pointed-to data
    std::vector<Timestamp> timestamps;

    /// @brief Construct an object specifying the number of timestamps to allocate
    /// @param count The number of timestamps
    DMsMessageReplies(std::uint8_t count) : padding_null_byte(0) {
        reply_count = count == 0 ? 0 : count - 1;
        timestamps.resize(reply_count, 0);
    }

    /// @brief Construct an object specifying a list of timestamps to allocate
    /// @param list The list of timestamps
    DMsMessageReplies(const std::vector<Timestamp>& list) : padding_null_byte(0), timestamps(list) {
        reply_count = list.empty() ? 0 : list.size() - 1;
    }

    /// @brief Return `true` if there are allocated timestamps. Note that the
    ///        `reply_count` member will always encode a non-zero value, but the
    ///        actual `timestamps` pointer could be `nullptr`
    inline constexpr operator bool() const noexcept {
        return timestamps.data() != nullptr;
    }

    /// @brief Return the size that a serialized payload would have
    inline constexpr std::size_t size() const noexcept {
        return 2 + (timestamps.empty() ?
            1 : // We need at least a value
            reply_count + 1 // Biased encoding
        ) * sizeof(Timestamp);
    }

    /// @brief Return a string of bytes containing a serialized version of this struct
    inline constexpr std::string serialize() noexcept {
        // Correct any mismatch in data (assuming that the timestamps vector is not empty else add a 0 element)
        if (timestamps.empty()) timestamps.push_back(0);
        reply_count = timestamps.size() - 1;

        std::string result(this->size(), '\0'); // Resize and zero out

        std::memcpy(result.data(), this, 2); // Copy the first two bytes

        if (!timestamps.data()) return result; // The timestamps are all zero since we haven't allocated anything
        std::memcpy(result.data() + 2, timestamps.data(), timestamps.size() * sizeof(Timestamp));

        return result;
    }
};

#pragma pack(push, 1)

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
};

/// @brief Convert in-place the endianness of a `DMsMessageAttachment` if the host is little endian
/// @param attachment The attachment to convert
template <>
inline constexpr void network_byte_order(DMsMessageAttachment& attachment) noexcept {
    if constexpr (std::endian::native != std::endian::little) return;
    attachment.id = std::byteswap(attachment.id);
    attachment.extension = static_cast<FileExtension>(std::byteswap(std::to_underlying(attachment.extension)));
}

/// @brief Return a value with the converted endianness of another value
///        of type `DMsMessageAttachment` if the host is little endian
/// @param attachment The attachment to convert
template <>
inline constexpr DMsMessageAttachment network_byte_order_copy(const DMsMessageAttachment& attachment) noexcept {
    if constexpr (std::endian::native != std::endian::little) return attachment;
    return {
        .id = std::byteswap(attachment.id),
        .extension = static_cast<FileExtension>(std::byteswap(std::to_underlying(attachment.extension)))
    };
}

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

    /// @brief Return `true` if the payload would be truncated even with long encoding
    inline constexpr bool would_truncate_payload() const noexcept {
        if (payload && // Only if we have a payload
            payload->data.size() > 2040 // More than the max value
        ) return true;
        return false; // Default case
    }

    /// @brief Return `true` if the length of the payload would require to use long encoding
    inline constexpr bool would_need_long_encoding() const noexcept {
        if (payload && // Only if we have a payload
            !header.length_encoding() && // If it's not long encoding already
            payload->data.size() > 1020 // More than the max value
        ) return true;
        return false; // Default case
    }

    /// @brief Return the size in bytes of a hypothetically serialized instance of this object
    inline constexpr std::size_t size() const noexcept {
        std::size_t result = sizeof(header);
        if (replies) result += replies->size();
        if (attachments) result += attachments->size() * sizeof(DMsMessageAttachment);
        if (payload)
            result += (this->would_need_long_encoding()
                    && this->would_truncate_payload()   ? // Change behaviour occasionally
                2040                                    : // Saturate the value
                payload->data.size()                      // Or just use a regular size
            );
        return result;
    }

    /// @brief Create a string of bytes representing a supposedly valid
    ///        message from this instance
    inline constexpr std::string serialize() {
        // Before doing anything, correct mismatching data
        DMsMessageMetadata meta = DMsMessageMetadata::from_metadata(header.metadata);

        if (!replies) meta.reply = false;
        if (payload) {
            meta.length = payload->data.size(); // NOT encoded length
            meta.length_encoding = this->would_need_long_encoding(); // Change it accordingly
            if (meta.length_encoding && meta.length > 2040) {
                payload->data.resize(2040); // Trucate data
                meta.length = 2040; // Saturated value
            }
        }

        if (attachments) {

            // Just clamp and truncate the number of attachments if too many
            if (attachments->size() > 7) {
                meta.attachment_count = 7;
                attachments->resize(7);
            } else meta.attachment_count = attachments->size();

        } else meta.attachment_count = 0; // Already 0

        header.metadata = meta.to_metadata(); // Write the encoded data back

        // We need to pad the bytes earlier so that the call to size takes them into account
        if (payload) {
            if (header.length_encoding())
                payload->long_encode();
            else
                payload->short_encode();
        }

        // Now we actually serialize
        std::string output(this->size(), 0);
        char* ptr = output.data();

        std::memcpy(ptr, &header, sizeof(header));
        ptr += sizeof(header);

        if (replies) {
            std::string data = replies->serialize();
            std::memcpy(ptr, data.data(), data.size());
            ptr += data.size();
        }

        if (attachments) {
            std::size_t bytes = attachments->size() * sizeof(attachments->front());
            std::memcpy(ptr, attachments->data(), bytes);
            ptr += bytes;
        }
    
        // If we truncated it it still is in range
        if (payload)
            std::memcpy(ptr, payload->data.data(), payload->data.size());

        return output;
    }
};


} // namespace ControlZ

#endif // CONTROLZ_ENVELOPE_HPP
