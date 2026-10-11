#include <iostream>
#include "../src/headers/WebSocket.hpp"
#include "test_support.hpp"

int main() {
    
    using namespace ControlZ::ws;

    // ======================================================
    // Packet header, metadata and flags tests
    // ======================================================

    PacketHeaderMetadata meta = {
        .type = PacketType::ManageFriend,
        .version = 0
    };
    REQUIRE(meta == PacketHeaderMetadata::from_metadata(meta.to_metadata()));

    PacketHeader header = {
        .metadata = meta.to_metadata(),
        .flags = PacketFlags::RespondAck | PacketFlags::RespondNack,
        .seq_id = 123,
        .user_id = 69,
        .payload_len = 5, // "nope!"
    };
    REQUIRE(header.verify() == PacketVerifyError::InvalidAckFlags);
    header.flags = PacketFlags::None;
    REQUIRE(header.verify() == PacketVerifyError::OK);

    auto enc = header.encode();
    REQUIRE(enc.has_value()); // Unnecessary
    std::string encoded = *enc;
    encoded += "nope"; // not 5 characters in size

    auto dec = PacketHeader::decode(encoded);
    REQUIRE(!dec.has_value()); // We should get the payload length error
    REQUIRE(dec.error() == PacketVerifyError::PayloadSizeMismatch);
    encoded += '!'; // Add one character
    dec = PacketHeader::decode(encoded);
    REQUIRE(dec.has_value()); // Now we're good

    PacketHeader decoded = *dec;
    REQUIRE(decoded == header); // We must not lose any data

    // Test for too short payloads
    std::string shorter = "hehe";
    auto fail = PacketHeader::decode(shorter);
    REQUIRE(!fail.has_value());
    REQUIRE(fail.error() == PacketVerifyError::PayloadIsTooShort);

    PacketHeader invalid = {
        .metadata = PacketHeaderMetadata { // Invalid fields
            .type = static_cast<PacketType>(std::to_underlying(CONTROLZ_MAX_PACKET_TYPE_VALUE) + 1),
            .version = 69
        }.to_metadata(),
        .flags = PacketFlags::None,
        .seq_id = 0,
        .payload_len = 0
    };
    PacketVerifyError errors = invalid.verify(); // We should have these two errors
    REQUIRE(errors & PacketVerifyError::InvalidVersion);
    REQUIRE(errors & PacketVerifyError::InvalidPacketType);

    // ======================================================
    // Packet frame tests
    // ======================================================

    Packet p = {
        .header = header,
        .payload = "hello"
    };
    REQUIRE(p.verify() == PacketVerifyError::OK);
    std::string encoded_p = *p.encode();
    auto decoded_p = Packet::decode(encoded_p);
    REQUIRE(decoded_p.has_value());
    REQUIRE(p == *decoded_p);

    // Empty payload
    p.header.payload_len = 0;
    p.payload.clear();
    REQUIRE(p.verify() == PacketVerifyError::OK);
    encoded_p = *p.encode();
    decoded_p = Packet::decode(encoded_p);
    REQUIRE(decoded_p.has_value());
    REQUIRE(p == *decoded_p);

    // ======================================================
    // Individual packet tests
    // ======================================================

    // Auth packet
    Auth auth_p = {
        .username_hash = {'6'},
        .pswd_hash = {'9'}
    };
    p.payload = auth_p.encode();
    p.header.payload_len = p.payload.size();
    REQUIRE(p.verify() == PacketVerifyError::OK);
    encoded_p = *p.encode();
    decoded_p = Packet::decode(encoded_p);
    REQUIRE(decoded_p.has_value());
    REQUIRE(p == *decoded_p);
    auto decoded_auth = Auth::decode(decoded_p->payload);
    REQUIRE(decoded_auth.has_value());
    REQUIRE(auth_p == *decoded_auth);

    // ManageFriend packet
    ManageFriend mfriend_p = {
        .target = 0,
        .action = ManageFriendAction::AcceptFriendRequest
    };
    auto status = mfriend_p.verify();
    REQUIRE(status & ManageFriendVerifyError::ZeroTargetUserID);
    mfriend_p.target = 69;
    status = mfriend_p.verify();
    REQUIRE(status == ManageFriendVerifyError::OK);
    p.payload = *mfriend_p.encode();
    p.header.payload_len = p.payload.size();
    REQUIRE(p.verify() == PacketVerifyError::OK);
    encoded_p = *p.encode();
    decoded_p = Packet::decode(encoded_p);
    REQUIRE(decoded_p.has_value());
    REQUIRE(p == *decoded_p);
    auto decoded_mfriend = ManageFriend::decode(decoded_p->payload);
    REQUIRE(decoded_mfriend.has_value());
    REQUIRE(mfriend_p == *decoded_mfriend);

    return 0;
}
