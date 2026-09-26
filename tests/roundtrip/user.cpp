#include <iostream>
#include <filesystem>
#include "../../src/headers/User.hpp"
#include "test_support.hpp"

int main() {
    using namespace ControlZ;

    const auto path = ControlZTests::temp_path("roundtrip.usr");
    ControlZTests::remove_if_present(path);

    UserHeaderMetadata metadata {
        .version = 0,
        .mod_level = 3,
        .bio_policy = 2,
        .friends_policy = 1,
        .user_status = 1
    };
    // Perform integrity checks across encoding and decoding
    const auto decoded_metadata = UserHeaderMetadata::from_metadata(metadata.to_metadata());
    REQUIRE(decoded_metadata.version == 0);
    REQUIRE(decoded_metadata.mod_level == 3);
    REQUIRE(decoded_metadata.bio_policy == 2);
    REQUIRE(decoded_metadata.friends_policy == 1);
    REQUIRE(decoded_metadata.user_status == 1);

    // Make up some data
    UserFile original {
        .header = {
            .user_id = 0x1234,
            .metadata = metadata.to_metadata(),
            .registration_timestamp = 99
        },
        .username = "Alice",
        .bio = "A short bio",
        .friends = { 4, 8, 15, 16 }
    };
    REQUIRE(original.size() % 2 == 0);
    REQUIRE(original.serialize().size() == original.size()); // Expect that the serialized size is actually what we predicted

    REQUIRE(create_user_file(path, original) == CreateUserFileError::OK);
    auto restored = deserialize_user_file(path);
    // Check for integrity once again
    REQUIRE(restored.has_value());
    REQUIRE(restored->header.user_id == original.header.user_id);
    REQUIRE(restored->header.registration_timestamp == original.header.registration_timestamp);
    REQUIRE(restored->username == original.username);
    REQUIRE(restored->bio == original.bio);
    REQUIRE(restored->friends == original.friends);

    // Other simple checks
    REQUIRE(create_user_file(path, original) == CreateUserFileError::FileAlreadyExists);
    REQUIRE(update_user_file(path, original) == CreateUserFileError::OK);

    UserHeader invalid = original.header;
    const_cast<char*>(invalid.magic)[0] = 'X'; // Corrupt it
    REQUIRE(invalid.verify() & VerifyUserHeaderError::InvalidMagic);

    // Again the last check
    REQUIRE(deserialize_user_file(ControlZTests::temp_path("missing.usr")).error() ==
        DeserializeUserFileError::FileDoesNotExist);

    // Clean up
    ControlZTests::remove_if_present(path);

    return 0;
}
