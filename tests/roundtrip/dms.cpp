#include <iostream>
#include <filesystem>
#include "../../src/headers/DMs.hpp"
#include "test_support.hpp"

int main() {
    using namespace ControlZ;

    // Check for context strings being expected values
    REQUIRE(derive_context_string('|', std::string("alice"), std::string("bob")) == "alice|bob");
    REQUIRE(derive_context_string('|', std::string("alone")) == "alone");

    const std::string plaintext("message\0payload", 15); // Add a sneaky null byte
    std::string encrypted = plaintext;
    const auto key = derive_key(1234, "alice|bob"); // The string from earlier
    // Circular integrity checks
    encrypt_decrypt(encrypted, key);
    REQUIRE(encrypted != plaintext);
    encrypt_decrypt(encrypted, key);
    REQUIRE(encrypted == plaintext);

    const auto path = ControlZTests::temp_path("roundtrip.dm");
    ControlZTests::remove_if_present(path);

    // First create a new file and then append various data samples
    REQUIRE(create_dms_file(path) == CreateDMsFileError::OK);
    REQUIRE(append_dms_file(path, std::string("first"),  111, "alice|bob") == AppendDMsFileError::MisalignedData);
    REQUIRE(append_dms_file(path, std::string("first!"), 111, "alice|bob") == AppendDMsFileError::OK);
    REQUIRE(append_dms_file(path, std::string("second"), 222, "alice|bob") == AppendDMsFileError::OK);

    // Forward-linked data
    auto forward = decrypt_dms_file(path, "alice|bob");
    REQUIRE(forward.has_value());
    REQUIRE(forward->size() == 2); // 2 messages total
    REQUIRE((*forward)[0] == std::string("first!", 6));
    REQUIRE((*forward)[1] == "second"); // Expect values
    REQUIRE(convert_dms_file(path, true) == ConvertDMsFileError::OK); // To backward-linked

    auto backward = decrypt_dms_file(path, "alice|bob");
    REQUIRE(backward.has_value());
    REQUIRE(backward->size() == 2);
    REQUIRE((*backward)[0] == "second"); // Expect values in reverse order
    REQUIRE((*backward)[1] == std::string("first!", 6));
    REQUIRE(convert_dms_file(path, true) == ConvertDMsFileError::TargetTypeIsCurrent);
    // Simple test
    REQUIRE(decrypt_dms_file(path, "alice|bob", 0, 0).error() == DecryptDMsFileError::ZeroMaxNodes);

    // Clean up
    ControlZTests::remove_if_present(path);

    return 0;
}
