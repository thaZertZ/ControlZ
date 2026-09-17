
# Changelog

## `src/headers/Common.hpp`:

- Added this header
- Created a function macro called
  `CONTROLZ_MAKE_SCOPED_ENUM(name, backing, def, zero, ...)`
  to create scoped enums the old way allowing for derived
  bitwise operations and comparison
- Added `FileExtension` enum class which will be filled through
  time with file extensions chosen from
  [here](https://en.wikipedia.org/wiki/Lists_of_filename_extensions).
- Added a safe `random_number` function returning an `std::uint64_t`.
- Added an `AllSameAs` concept for variadic argument type checks.

## `src/headers/Envelope.hpp`:

- Reimplemented ***everything*** and kept old version in
  `src/headers/old/Envelope.hpp`.

## `src/headers/DMs.hpp`:

- Reimplemented ***everything*** and kept old version in
  `src/headers/old/DMs.hpp`.

## `docs/`:

- Added specifications and documentation for `Envelope.hpp` and `DMs.hpp`.
