
# Changelog

## Global:

- Updated serialization logic across all formats where needed
- Implemented `network_byte_order()` and `network_byte_order_copy()`
  for all types that required it
- Created hopefully exhaustive tests for everything

## `src/docs/User.md`:

- Changed the header specification to have the second padding null byte better
  aligned to the username length field.

## `src/headers/Common.hpp`:

- Changed `network_byte_order()` to accept only integral types by default
- Overloaded `network_byte_order()` to accept an `std::string` template parameter
  used for converting individually serialized data. When using it on structs
  that do not overload it, simply call `.serialize()` on them first and then pass
  the string to `network_byte_order()`
- Changed overloads of `network_byte_order()` not returning `void` to be named
  `network_byte_order_copy()` for better readability and less ambiguity to compile
- Added an overload to unary `operator+()` for `CONTROLZ_MAKE_SCOPED_ENUM()` that
  returns the held value as the backing type

## `src/headers/Envelope.hpp`:

- Removed `#pragma pack` from `DMsMessageReplies` since it contained a vector
  therefore could not be serailized by `memcpy`ing
- Removed a constructor of `DMsMessageAttachment` that made unclear the initialization
  of a struct if its type by using only one if its members in the initializer
  instead of two
- Added a constructor to `DMsMessageReplies` that accepts an `std::vector<Timestamp>`
- Added two static utility methods to `DMsMessageMetadata` to encode and decode a length
- Made `const` the padding byte in `DMsMessageReplies`
- Fixed some problems due to alignment and padding in `DMsMessageReplies::size()` and
  `serialize()`
- Added `would_truncate_payload()` and `would_need_long_encoding()` queries to `DMsMessage`
  used in other methods
- Added a `size()` method to `DMsMessage` that takes into account changes that would be
  made during serialization
- Fixed `DMsMessage::serialize()` to take alignment and padding into account and changing
  internal state properly to adapt to inconsistent data

## `src/headers/DMs.hpp`:

- Added an interpolation character to the first parameter of `derive_context_string()`
- Fixed incorrect integrity checks when appending or decrypting a DMs file
- Fixed file control flow inside `append_dms_file()` and `decrypt_dms_file()`

## `src/headers/User.hpp`:

- Reflected the small change made to the `UserHeader` specification (moved `padding2` to byte 77)
- Changed `UserHeader::serialize()` to rewrite magic bytes with ugly `const_cast`s
- Fixed inconsistent size problems in file logic and serialization logic
