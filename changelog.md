
# Changelog

## `src/headers/Common.hpp`:

- Fixed the `network_byte_order()` overload for `std::string` to
  reverse its bytes instead of switching places every 2 characters
- Added a hopefully unambiguous overload of `network_byte_order()`
  to convert the endianness of the elements of an `std::vector`

## `tests/Common.hpp`:

- Added tests for `network_byte_order()` overloads for `std::string`
  and `std::vector`

## `docs/WebSocket.md`:

- Improved the specification (again)

## `docs/SequenceID.md`:

- Specified the behaviour of packet ID tracking

## `src/headers/WebSocket.hpp`:

- Added new packets from the specification like `Heartbeat` and `Explode`
