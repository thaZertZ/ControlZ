#include <iostream>
#include <vector>
#include "../../src/headers/Mappings.hpp"
#include "test_support.hpp"

int main() {
    using namespace ControlZ;

    AttachmentMapping even { .id = 0, .mapping = "file.txt" }; // Even number of characters
    AttachmentMapping odd  { .id = 1, .mapping = "x" }; // Odd
    // Test serialize to deserialize integrity
    REQUIRE(AttachmentMapping::from_bytes(even.serialize()).id == even.id);
    REQUIRE(AttachmentMapping::from_bytes(even.serialize()).mapping == even.mapping);
    odd.align();
    REQUIRE(odd.size() % 2 == 0);
    REQUIRE(AttachmentMapping::from_bytes(odd.serialize()).mapping == odd.mapping);

    // Do the same integrity check for header config
    MapHeaderConfig config { .version = 0, .full = true, .last_slot = 17 };
    const auto decoded_config = MapHeaderConfig::from_config(config.to_config());
    REQUIRE(decoded_config.version == config.version);
    REQUIRE(decoded_config.full == config.full);
    REQUIRE(decoded_config.last_slot == config.last_slot);

    MapHeader bad_header;
    const_cast<char*>(bad_header.magic)[1] = 'X'; // Currupt it
    REQUIRE(bad_header.verify() & MapHeaderVerifyError::InvalidMagic);

    const auto path = ControlZTests::temp_path("roundtrip.map");
    ControlZTests::remove_if_present(path);
    REQUIRE(create_map_file(path) == CreateMapFileError::OK);
    REQUIRE(append_map_file(path, even) == AppendMapFileError::OK);

    std::vector<AttachmentMapping> mappings {
        // Only sequential IDs are allowed
        AttachmentMapping { .id = 2, .mapping = "a.bin" },
        AttachmentMapping { .id = 3, .mapping = "b.bin" },
        AttachmentMapping { .id = 4, .mapping = "c.bin" }
    };
    for (auto& mapping : mappings) mapping.align();
    REQUIRE(append_map_file(path, mappings) == AppendMapFileError::OK);

    // Deserialize every file mapping, remember that `odd` was never serialized
    auto restored = deserialize_map_file<AttachmentMapping>(path);
    REQUIRE(restored.has_value());
    REQUIRE(restored->size() == 4);
    REQUIRE((*restored)[0].id == 0);
    REQUIRE((*restored)[1].mapping == std::string("a.bin\0", 6)); // Remember the padding null byte
    REQUIRE((*restored)[3].id == 4);

    // Deserialize the mappings partially
    auto subset = deserialize_map_file<AttachmentMapping>(path, true, 1, 2);
    REQUIRE(subset.has_value());
    REQUIRE(subset->size() == 2);
    REQUIRE((*subset)[0].id == 2); // We have an offset of 2, also because `odd` was never serialized
    REQUIRE((*subset)[1].id == 3);

    // The file should not exist
    REQUIRE(deserialize_map_file<AttachmentMapping>(
        ControlZTests::temp_path("missing.map")).error() == DeserializeMapFileError::FileDoesNotExist);

    // Clean up
    ControlZTests::remove_if_present(path);

    return 0;
}
