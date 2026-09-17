#include <iostream>
#include "../../headers/Mappings.hpp"

int main() {

    auto create_errors = ControlZ::create_map_file("test.map", true);

    if (!create_errors) std::cout << "Success creating the file\n";
    else {
        std::cout << "Failed creating the file\n";
        return create_errors;
    }

    std::vector<ControlZ::AttachmentMapping> mappings(300, ControlZ::AttachmentMapping());

    for (std::size_t i = 0; i < 300; ++i) {
        ControlZ::AttachmentMapping& mapping = mappings[i];
        mapping.id = i;
        mapping.mapping = std::to_string(i) + ".txt";
        mapping.align();
    }

    auto append_errors = ControlZ::append_map_file("test.map", mappings);

    if (!append_errors) std::cout << "Success appending\n";
    else {
        std::cout << "Failed appending\n";
        return append_errors;
    }

    auto deserialize_errors = ControlZ::deserialize_map_file<ControlZ::AttachmentMapping>("test.map");

    if (deserialize_errors.has_value()) {
        std::cout << "Success deserializing\n";
        const auto& mappings = deserialize_errors.value();
        for (const auto& mapping : mappings)
            std::cout << mapping.id << " | " << mapping.mapping << "\n";
    } else {
        std::cout << "Failed deserializing\n";
        return deserialize_errors.error();
    }

    return 0;
}
