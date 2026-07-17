
# Todo

## `src/headers/Envelope.hpp`:

- Try to change every timestamp field from a 32bit integer to a 64bit
  integer representing milliseconds (create a new dev branch).

## `src/headers/DMs.hpp`:

- Try to adapt to the changes made to `Envelope.hpp` (on the same branch).

## `src/headers/Util.hpp`:

- Make conversion functions for from and to Base32
- Make a utility function to derive the custom timestamp (64bit integer)
  for the Base32 representation of milliseconds (with epoch at
  `00:00:00 Jan 2026 UTC`, probably locales needed).

## `docs/`:

- Create documentation for the already existing APIs (`Envelope.hpp`,
  `DMs.hpp`).

## `docs/WebSocket.md`:

- Specify the use of `Ack` and `Nack` packets in response to other types of packets
- Change the `Ack` and `Nack` specification according to the needs of other packet
  types with one or more bytes specifying an enum (class) value for the response.

## `src/headers/UserSession.hpp`:

- Create this header
- Improve the old API and binary format
- Provide serialization functions for `InfoUserResponse` and `InfoChatResponse` packets
