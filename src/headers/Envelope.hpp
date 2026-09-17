#ifndef CONTROLZ_ENVELOPE_HPP
#define CONTROLZ_ENVELOPE_HPP

#include "Common.hpp"
#include <expected>
#include <optional>
#include <vector>
#include <string>
#include <cstring>

namespace ControlZ {


CONTROLZ_MAKE_SCOPED_ENUM (

    DMsMetadataError, // Type name
    std::uint8_t, // Backing type
    OK, // Default value
    OK, // Zero value

    // Enum values
    OK                      = 0,
    ZeroVersion             = 1 << 0,
    VersionTooHigh          = 1 << 1,
    AttachmentCountTooHigh  = 1 << 2,
    ReplyAndEditNotAllowed  = 1 << 3,
    LengthNeedsLongEncoding = 1 << 4
)

struct DMsMessageMetadata {
    bool edit = false;
    std::uint8_t version = 0;
    std::uint8_t attachment_count = 0;
    bool reply = false;
    bool length_encoding = false;
    std::uint16_t length = 0;
};

#pragma pack(push, 1)

struct DMsMessageHeader {
    Timestamp timestamp = 0;
    UserID user_id = 0;
    std::uint16_t metadata = 0;

    DMsMessageHeader() noexcept = default;

    DMsMessageHeader(Timestamp t, UserID uid, std::uint16_t md) noexcept
        : timestamp(t), user_id(uid), metadata(md) {}

    inline constexpr std::size_t size() const noexcept { return 8; }

    inline constexpr bool length_encoding() const noexcept {
        return ((metadata & (0b1 << 0x8)) >> 0x8) != 0;
    }

    inline constexpr DMsMessageMetadata get_metadata() const noexcept {
        DMsMessageMetadata result;

        result.edit = metadata >> 0xF;
        result.version = (metadata & (0b11 << 0xD)) >> 0xD;
        result.attachment_count = (metadata & (0b111 << 0xA)) >> 0xA;
        result.reply = ((metadata & 0x9) >> 0x9) != 0;
        result.length_encoding = ((metadata & 0x8) >> 0x8) != 0;
        if (result.length_encoding)
            result.length = (metadata & 0xFF) * 8;
        else
            result.length = (metadata & 0xFF) * 4;

        return result;
    }
};

inline constexpr std::expected<std::uint16_t, DMsMetadataError> create_metadata(DMsMessageMetadata meta) noexcept {

    DMsMetadataError errors;

    if (meta.version == 0) errors |= DMsMetadataError::ZeroVersion;
    if (meta.version > 4) errors |= DMsMetadataError::VersionTooHigh;
    if (meta.attachment_count > 7) errors |= DMsMetadataError::AttachmentCountTooHigh;
    if (meta.edit && meta.reply) errors |= DMsMetadataError::ReplyAndEditNotAllowed;
    if (!meta.length_encoding && meta.length > 1020) errors |= DMsMetadataError::LengthNeedsLongEncoding;

    if (errors) return std::unexpected<DMsMetadataError>(errors);

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
    if (meta.length_encoding) // Long encoding
        result |= (meta.length + 7) / 8; // No shifts, all 8 bits
    else // Short encoding
        result |= (meta.length + 3) / 4; // Same

    result |= (meta.length_encoding ? 1 : 0) << 0x8; // 1 bit
    result |= (meta.reply ? 1 : 0)           << 0x9; // 1 bit
    result |= meta.attachment_count          << 0xA; // 3 bits
    result |= meta.version                   << 0xD; // 2 bits
    result |= (meta.edit ? 1 : 0)            << 0xF; // 1 bit

    return result;
}

struct DMsMessageReplies {
    std::uint8_t reply_count = 0; // Encodes 1
    std::uint8_t padding_null_byte = 0;
    Timestamp* timestamps = nullptr;

    DMsMessageReplies() noexcept = default;
    ~DMsMessageReplies() noexcept {
        if (timestamps) delete[] timestamps;
    }

    DMsMessageReplies(std::uint8_t count) {
        reply_count = count == 0 ? 0 : count - 1;
        padding_null_byte = 0;
        timestamps = new Timestamp[reply_count];
    }

    inline constexpr operator bool() const noexcept {
        return timestamps != nullptr;
    }

    inline constexpr std::size_t size() const noexcept {
        return 2 + (reply_count + 1 /* biased encoding */);
    }
};

struct DMsMessageAttachment {
    AttachmentID attachment_id = 0;
    FileExtension extension = FileExtension::None;

    DMsMessageAttachment() noexcept = default;
    ~DMsMessageAttachment() noexcept = default;

    DMsMessageAttachment(AttachmentID aid) noexcept
        : attachment_id(aid) {}
    DMsMessageAttachment(AttachmentID aid, FileExtension ext) noexcept
        : attachment_id(aid), extension(ext) {}

    inline constexpr std::size_t size() const noexcept { return 6; }
};

struct DMsMessagePayload {
    std::string data;

    DMsMessagePayload() noexcept = default;
    ~DMsMessagePayload() noexcept = default;

    DMsMessagePayload(const std::string& str) : data(str) {}

    inline constexpr void short_encode() {
        std::uint8_t exceeding = data.size() % 4;
        if (exceeding == 0) return;
        data.append(4 - exceeding, '\0');
    }
    inline constexpr void long_encode() {
        std::uint8_t exceeding = data.size() % 8;
        if (exceeding == 0) return;
        data.append(8 - exceeding, '\0');
    }

    inline constexpr std::size_t size() const noexcept { return data.size(); }
};

#pragma pack(pop)

struct DMsMessage {
    DMsMessageHeader header;
    std::optional<DMsMessageReplies> replies;
    std::optional<std::vector<DMsMessageAttachment>> attachments;
    std::optional<DMsMessagePayload> payload;

    inline constexpr std::vector<std::uint8_t> serialize() {
        std::vector<std::uint8_t> output;

        std::size_t final_size = header.size();
        if (replies) final_size += replies.value().size();
        if (attachments)
            final_size += attachments.value().size() * attachments.value().front().size();
        if (payload) final_size += payload.value().size();

        output.resize(final_size, 0);
        std::uint8_t* ptr = output.data();

        std::memcpy(ptr, &header, header.size());
        ptr += header.size();

        if (replies) {
            std::memcpy(ptr, &replies.value(), 2);
            ptr += 2;
            std::size_t bytes = replies.value().reply_count + 1;
            std::memcpy(ptr, replies.value().timestamps, bytes);
            ptr += bytes;
        }

        if (attachments) {
            std::size_t bytes = attachments.value().size() * attachments.value().front().size();
            std::memcpy(ptr, attachments.value().data(), bytes);
            ptr += bytes;
        }

        if (payload) {
            if (header.length_encoding())
                payload.value().long_encode();
            else
                payload.value().short_encode();

            std::memcpy(ptr, payload.value().data.data(), payload.value().size());
        }

        return output;
    }
};


} // namespace ControlZ

#endif // CONTROLZ_ENVELOPE_HPP
