
# Todo

## Global:

- Change the `append_*` functions for binary formats to align
  data before writing to follow the serialization boundary sanitization
  mentioned earlier
- Adapt all binary formats, their serialization and deserialization
  functions to convert to and from big endian with `network_byte_order()`
  overloads to be sure that when sending data over the network it is always
  ready to be sent no matter the endianness of the host sending it

## `src/headers/WebSocket.hpp`:

- Finish implementing all packet types with the same API

## `docs/WebSocket.md`:

- Keep reviewing the specification according to the needs of implementations
