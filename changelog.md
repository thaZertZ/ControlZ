
# Changelog

## `docs/User.md`:

- Added a **friend request policy** field to the user format
- Added a **message exchange policy** field too

## `docs/Chat.md`:

- Added this file as a specification for a custom format holding
  public chat metadata and config

## `docs/WebSocket.md`:

- Updated part of the packets' specification and added a new
  `ManageFriend` packet

## `src/headers/Common.hpp`:

- Added a new type alias `ChatID` that corresponds to a 32bit
  unsigned integer
- Updated the `CONTROLZ_MAKE_SCOPED_ENUM()` macro to create a
  constructor that accepts a value of the backing type

## `src/headers/Chat.hpp`:

- Added this file to implement the specification in `docs/Chat.md`

## `src/headers/DMs.hpp`:

- Added two more optional arguments to `convert_dms_file()` to
  allow a filename to be specified for the file to which to the
  converted file to

## `src/headers/User.hpp`:

- Added a whole API for writing `UserID` pairs for friend requests
  to binary blob files
- Updated the `UserHeaderMetadata` struct to also pack the new
  **friend request policy** and **message exchange policy** fields

## `tests/roundtrip/run.bat`:

- Updated this script to work better with failing tests and tests
  that don't compile

## `tests/roundtrip/dms.cpp`:

- Updated test to check both in-place and non-in-place linking type
  conversions

## `tests/roundtrip/user.cpp`:

- Added extra tests for the friend request files API in `User.hpp`
