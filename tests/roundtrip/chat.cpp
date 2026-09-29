#include <iostream>
#include "../../src/headers/Chat.hpp"
#include "test_support.hpp"

int main() {

    using namespace ControlZ;

    auto path = ControlZTests::temp_path("test.chat");

    ChatHeaderMetadata meta = {
        .version = 0,
        .requires_password = true,
        .name_flag = true,
        .desc_flag = true,
        .members_flag = false,
    };
    auto same = ChatHeaderMetadata::from_metadata(meta.to_metadata());
    REQUIRE(std::memcmp(&meta, &same, sizeof(meta)) == 0);

    ChatFile file = {
        .header = {
            .chat_id = 123,
            .metadata = meta.to_metadata(),
            .name_len = 12, // Length of "test chat :)"
            .desc_len = 42, // Length of the desc
            .members_len = 2
        },
        .name = "test chat :)",
        .description = "this is a test to see if this format works",
        .members = { 69, 42 },
        .pswd_data = std::make_optional<std::array<char, 64>>({'A'}) // All 'A's right?
    };

    REQUIRE(create_chat_file(path, file) == CreateChatFileError::OK);
    auto result = deserialize_chat_file(path);
    REQUIRE(result.has_value());
    auto data = result.value();
    // The exact same
    REQUIRE(file.header.chat_id == data.header.chat_id);
    REQUIRE(file.header.metadata == data.header.metadata);
    REQUIRE(file.header.name_len == data.header.name_len);
    REQUIRE(file.header.desc_len == data.header.desc_len);
    REQUIRE(file.header.members_len == data.header.members_len);
    REQUIRE(file.name == data.name);
    REQUIRE(file.description == data.description);
    REQUIRE(file.members == data.members);
    REQUIRE(data.pswd_data.has_value());
    REQUIRE(*file.pswd_data == *data.pswd_data); // Also expect a value

    // Change one single field
    file.description = "this is a second test to see if this format works";
    file.header.desc_len = 49; // Size of this ^^^^^^
    REQUIRE(update_chat_file(path, file) == CreateChatFileError::OK);
    auto edited = deserialize_chat_file(path);
    REQUIRE(edited.has_value());
    auto edit = edited.value();
    // The exact same
    REQUIRE(file.header.chat_id == data.header.chat_id);
    REQUIRE(file.header.metadata == data.header.metadata);
    REQUIRE(file.header.name_len == data.header.name_len);
    REQUIRE(file.header.desc_len != data.header.desc_len); // NOT the same
    REQUIRE(file.header.members_len == data.header.members_len);
    REQUIRE(file.name == edit.name);
    // The changed value
    REQUIRE("this is a second test to see if this format works" == edit.description);
    REQUIRE(file.members == edit.members);
    REQUIRE(edit.pswd_data.has_value());
    REQUIRE(*file.pswd_data == *edit.pswd_data); // Also expect a value

    // Clean up
    ControlZTests::remove_if_present(path);

    return 0;
}
