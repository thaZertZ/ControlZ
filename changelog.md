
# Changelog

## `docs/WebSocket.md`:

- Added a new `GetFilename` packet type that is used to
  retrieve the attachment mapping of a file

## `src/headers/WebSocket.hpp`:

- Implemented `GetFilename` and its response

## `src/headers/Chat.hpp`:

- Added a binary deserializaton factory function to `ChatFile`
  and used it in the `deserialize_chat_file()` function

## `src/headers/Envelope.hpp`:

- Updated length encoding helpers to be easier to understand
- Added a binary deserialization factory function to
  `DMsMessageReplies` and `DMsMessage`
