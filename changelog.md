
# Changelog

## Global:

- Removed `cpp-httplib` for now and also removed chat tests
- Gave better defaults to metadata fields
- Added `normalize()` methods to all binary format structs

## `src/headers/Common.hpp`:

- Fixed the `network_byte_order()` overload for `std::string` to
  reverse its bytes instead of switching places every 2 characters

## `src/headers/Chat.hpp`:

- Overloaded `network_byte_order()` and `network_byte_order_copy()`
  when needed

## `tests/`:

- Removed chat tests and moved all other tests to this directory

## `docs/WebSocket.md`:

- Improved the specification

## `src/headers/WebSocket.hpp`:

- Started implementing the specification with fixed-size packets

## `tests/websocket.cpp`:

- Started testing the implementaton

## `tests/run.bat`:

- Improved the building and analysis logic to report compilation
  and execution status better
