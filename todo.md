
# Todo

## Global:

Adapt class and struct APIs to be more like Rust's with `from_` and `to_` factory
functions.

## `src/headers/Envelope.hpp`:

- Add comments for documentation

## `src/headers/Mappings.hpp`:

- Add a version of `append_map_file()` taking in a `const std::vector<Mapping>&`
- Add comments for documentation

## `docs/WebSocket.md`:

- Specify the use of `Ack` and `Nack` packets in response to other types of packets
- Change the `Ack` and `Nack` specification according to the needs of other packet
  types with one or more bytes specifying an enum (class) value for the response.
