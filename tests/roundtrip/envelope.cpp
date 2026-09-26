#include <iostream>
#include <cstring>
#include "../../src/headers/Envelope.hpp"
#include "test_support.hpp"

int main() {
    using namespace ControlZ;

    for (std::uint16_t length : { 0, 1, 4, 5, 1020, 1021, 2040 }) for (bool long_encoding : { false, true }) {
        if (!long_encoding && length > 1020) break; // Skip the iterations that would fail the last two length tests
        // Dummy data
        DMsMessageMetadata metadata = {
            .edit = true,
            .version = 2, // This should be invalid
            .attachment_count = 7, // Max value
            .reply = true,
            .length_encoding = long_encoding,
            .length = length
        };
        // Integrity check
        const auto decoded = DMsMessageMetadata::from_metadata(metadata.to_metadata());
        REQUIRE(decoded.edit);
        REQUIRE(decoded.version == metadata.version);
        REQUIRE(decoded.attachment_count == metadata.attachment_count);
        REQUIRE(decoded.reply);
        REQUIRE(decoded.length_encoding == long_encoding);
        // The last 2 cases would break
        REQUIRE(decoded.length == DMsMessageMetadata::decoded_length(length, long_encoding));
    }

    DMsMessageHeader native_header {
        .timestamp = 0x12345678,
        .user_id = 0x2345,
        .metadata = 0xA5C3
    };
    DMsMessageHeader network_header = native_header;
    network_byte_order<DMsMessageHeader>(network_header);
    network_byte_order<DMsMessageHeader>(network_header);
    REQUIRE(network_header.timestamp == native_header.timestamp);
    REQUIRE(network_header.user_id == native_header.user_id);
    REQUIRE(network_header.metadata == native_header.metadata);

    DMsMessageReplies replies(3);
    replies.timestamps = { 11, 22, 33 };
    const std::string reply_bytes = replies.serialize();
    REQUIRE(reply_bytes.size() == 2 + 3 * sizeof(Timestamp)); // This could just be `replies.size()` too
    REQUIRE(static_cast<std::uint8_t>(reply_bytes[0]) == 2); // Check for expected values in individual bytes
    REQUIRE(static_cast<std::uint8_t>(reply_bytes[1]) == 0);

    DMsMessage message = {
        .header = {
            .timestamp = 9,
            .user_id = 17,
            .metadata = DMsMessageMetadata {
                .length_encoding = false
            }.to_metadata()
        },
        .replies = std::make_optional<DMsMessageReplies>( DMsMessageReplies { { 101, 202 } }),
        .attachments = std::vector<DMsMessageAttachment> { { 42, FileExtension::CPP } },
        .payload = DMsMessagePayload { "hello" }
    };

    // Serialize and then check for expected values
    const std::string serialized = message.serialize();
    const auto decoded_metadata = DMsMessageMetadata::from_metadata(message.header.metadata);
    REQUIRE(!decoded_metadata.reply); // We didn't specify this flag in the initializer
    REQUIRE(decoded_metadata.attachment_count == 1);
    REQUIRE(decoded_metadata.length == 8);
    REQUIRE(serialized.size() == message.size());

    return 0;
}
