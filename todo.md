
# Todo

## Global:

- Change the `append_*` functions for binary formats to align
  data before writing to follow the serialization boundary sanitization
  mentioned earlier

## `src/headers/DMs.hpp`:

- Add an argument to `convert_dms_file()` of type `std::optional<std::fs::path>`
  called `opt_new_file` that defaults to `std::nullopt_t`, which would copy the
  contents of the given `path` parameter to *its* path (checking if it exists
  and overwriting it based on a fourth `bool` argument called `force_overwrite`,
  defaulting to `false`) and then changing `path` to the target `opt_new_file`
  path to seamlessly integrate with the current implementation

## `docs/WebSocket.md`:

- Finish the specification with the rest of the packets
